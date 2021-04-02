#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <vector>

#include <Windows.h>
#include <tlhelp32.h>

// NOTE: im pretty sure that the problem of tracking position of player is already solved in a better and less broken way than here by
// cheat engine and what not. Code below is pretty much me blindly exploring and finding a minimum working solution that is broken in
// many ways (and can even crash your pc).

// NOTE: the bug with recording sometimes stopping after death is because of page relocations which move PCHARACTER too far
// from the start of process memory. The way the tool works is it needs to have an upper limit on how long to scan pages before giving
// up because during character death the PCHARACTER structure doesn't exist in memory so we need to avoid wasting a minute or 
// more scanning all pages of the process. For that reason there is a sanity_check_time which is set to 4 times how long the first
// PCHARACTER search took. Since page relocation might make the needed search time to be larger than this limit, the searches are stopped
// early and PCHARACTER is not found.

struct Page {
  char* start;
  size_t size;
};


const char* output_path = "positions.txt";
const char* settings_path = "settings.txt";

// Limit max search time in case PCHARACTER is not there(after death for example) to avoid scanning all pages which takes a ton of time
// Start at basically infinite time, then calculate actual time from the length the first search
clock_t sanity_check_time = 1000 * 1000;
bool need_to_get_sanity_check_time = true;
// NOTE: don't want to check too often to avoid adding too much overhead to search, but also have to check often enough to not go too far past 1 second search boundary, for reference: largest observed page size is 100kk
size_t inpage_sanity_check_interval = 100000;

const clock_t no_writes_alarm_interval = 30 * 1000;

const int PSTATUS_name_offset = 0xA0;
// PSTATUS stats = [health current, health max, health max unbuffed, mana current, mana max, mana max unbuffed, 0000(IMPORTANT!!!), stamina current, stamina max, stamina max unbuffed]
const int PSTATUS_stats_offset = 0x0C;
// PCHRACTER stats = [health current, health max, mana current, mana max, stamina current, stamina max]
const int PCHARACTER_stats_offset = 0x2D4;
int PCHARACTER_x_offsets[] = {0x28, 0x1C, 0x10};
int PCHARACTER_y_offsets[] = {0x28, 0x1C, 0x14};
int PCHARACTER_z_offsets[] = {0x28, 0x1C, 0x18};

int read_bytes(char* data, int bytes) {
    int result = 0;
    for (int i = 0; i < bytes; i++) {
        result |= ((data[i] & 0xFF) << (8 * i));
    }

    return result;
}

bool chase_pointer(int* result, HANDLE pHandle, int base_address, int offsets[], int offsets_length) { 
    int address = base_address;
    DWORD total = 4;
    for (int i = 0; i < offsets_length; i++) { 
        address += offsets[i];
        if (!ReadProcessMemory(pHandle, (LPCVOID)address, &address, 4, &total) || total != 4) {
            return false;
        }
    } 
    *result = address; 
    return true;
}

float hex_to_float(int x) {
    return *((float*)&x);
}

int main(int, char**) {
    // Try to run on secondary core
    SetThreadIdealProcessor(GetCurrentThread(), MAXIMUM_PROCESSORS - 1);

    bool print_debug;
    int PSTATUS = -1;
    int PCHARACTER = -1;
    Page PCHARACTER_page = {
        start: NULL,
        size: 0,
    };
    int health_max = 0;
    int stamina_max = 0;
    int mana_max = 0;

    int interval = -1;
    char character_name[100];
    int given_health_max = -1;

    // Load settings
    {
        FILE* file = fopen(settings_path, "r");
        if (file == NULL) {
            printf("Failed to open settings file %s\n", settings_path);
            exit(1);
        }

        char buffer[100];
        while (true) {
            int scan_result = fscanf(file, "%s", buffer);
            if (scan_result == -1) {
                break;
            }

            if (strcmp(buffer, "#interval_in_milliseconds") == 0) {
                fscanf(file, " = %d\n", &interval);
            } else if (strcmp(buffer, "#name") == 0) {
                fscanf(file, " = %s\n", character_name);
            } else if (strcmp(buffer, "#health_max") == 0) {
                fscanf(file, " = %s\n", buffer);
                given_health_max = atoi(buffer);
            } else if (strcmp(buffer, "#print_debug") == 0) {
                int debug_int;
                fscanf(file, " = %d\n", &debug_int);
                if (debug_int == 0) {
                    print_debug = false;
                } else {
                    print_debug = true;
                }
            }
        }
        fclose(file);
    }

    if (given_health_max == -1) {
        printf("No health defined or incorrect value: %d\n", given_health_max);
        exit(1);
    }

    if (interval == -1) {
        printf("No interval defined or incorrect value: %d\n", interval);
        exit(1);
    }

    // In memory, chracter name is padded with '\0' after each char
    char* search_name = (char*)malloc(strlen(character_name) * 2);
    size_t search_name_length = strlen(character_name) * 2 - 1;
    for (size_t i = 0, j = 0; i < strlen(character_name); i++, j+=2) {
        search_name[j] = character_name[i];
        search_name[j + 1] = '\0';
    }

    size_t page_buffer_size = 134221824;
    char* page_buffer = (char*)malloc(page_buffer_size);

    // Get pid of process
    const char* process_name = "DARKSOULS.exe";
    DWORD pid = 0;
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(snapshot, &entry) == true) {
        while (Process32Next(snapshot, &entry) == true) {
            if (strncmp(entry.szExeFile, process_name, strlen(entry.szExeFile)) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        }
    }
    CloseHandle(snapshot);

    if (pid == 0) {
        printf("Failed to find process name \"%s\"\n", process_name);
        return 0;
    }

    while (true) {
        // Open process
        HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
        if (!handle) {
            printf("Failed to open process\n");
            return 0;
        }

        // Get page addresses
        std::vector<Page> pages;
        MEMORY_BASIC_INFORMATION page;
        char* address = NULL;
        bool found_PCHARACTER_page = false;
        while (VirtualQueryEx(handle, address, &page, sizeof(page)) == sizeof(page)) {
            if (page.State == MEM_COMMIT && page.Type == MEM_PRIVATE && page.Protect == PAGE_READWRITE) {
                pages.push_back({
                    start: address, 
                    size: page.RegionSize,
                });

                if (PCHARACTER_page.start == address && PCHARACTER_page.size == page.RegionSize) {
                    found_PCHARACTER_page = true;
                }
            }
            address = (char*)page.BaseAddress + page.RegionSize;
        }

        // Make sure that PCHARACTER page is still there, otherwise invalidate PCHARACTER
        if (!found_PCHARACTER_page) {
            PCHARACTER = -1;
        }

        // Make sure page buffer fits largest page
        size_t largest_size = 0;
        for (size_t i = 0; i < pages.size(); i++) {
            Page page = pages[i];

            if (page.size > largest_size) {
                largest_size = page.size;
            }
        }
        if (largest_size > page_buffer_size) {
            page_buffer = (char*)realloc(page_buffer, largest_size);
            page_buffer_size = largest_size;
        }

        // Search for PSTATUS once at the start
        if (PSTATUS == -1) {
            for (size_t i = 0; i < pages.size(); i++) {
                Page page = pages[i];

                DWORD total;
                if (!ReadProcessMemory(handle, page.start, page_buffer, page.size, &total)) {
                    printf("Failed to read process memory\n");
                    break;
                }

                // There are other memory locations containing the character name which are not PSTATUS, so need to confirm that a location is the real PSTATUS by checking if health max equals the given one
                for (size_t j = 0; j < total - search_name_length; j += 4) {
                    if (memcmp(page_buffer + j, search_name, search_name_length) == 0) {
                        char* PSTATUS_ptr = page_buffer + j - PSTATUS_name_offset;

                        int this_health_max = read_bytes(PSTATUS_ptr + PSTATUS_stats_offset + 4, 4);

                        if (this_health_max == given_health_max) {
                            PSTATUS = (int)page.start + j - PSTATUS_name_offset;

                            // Print double bell when PSTATUS is found
                            printf("\a\a");

                            break;
                        }
                    }
                }

                if (PSTATUS != -1) {
                    break;
                }
            }

            if (PSTATUS == -1) {
                printf("Failed to find PSTATUS. Make sure that the character name and health max is correct and that the game is running and you are logged into character.\n");
                exit(1);
            } else {
                printf("Found PSTATUS, starting to record.\n");
            }
        }

        // Update stat maxes
        ReadProcessMemory(handle, (LPCVOID)(PSTATUS + PSTATUS_stats_offset + 4), &health_max, 4, NULL); 
        ReadProcessMemory(handle, (LPCVOID)(PSTATUS + PSTATUS_stats_offset + 3 * 4), &mana_max, 4, NULL); 
        ReadProcessMemory(handle, (LPCVOID)(PSTATUS + PSTATUS_stats_offset + 8 * 4), &stamina_max, 4, NULL); 

        // Check if can reuse previous search result, stat maxes must be the same
        if (PCHARACTER != -1) {
            int this_health_max = 0;
            int this_mana_max = 0;
            int this_stamina_max = 0;
            DWORD total;
            bool read_success = ReadProcessMemory(handle, (LPCVOID)(PCHARACTER + PCHARACTER_stats_offset + 4), &this_health_max, 4, &total); 
            read_success &= (total == 4);
            read_success &= ReadProcessMemory(handle, (LPCVOID)(PCHARACTER + PCHARACTER_stats_offset + 3 * 4), &this_mana_max, 4, &total); 
            read_success &= (total == 4);
            read_success &= ReadProcessMemory(handle, (LPCVOID)(PCHARACTER + PCHARACTER_stats_offset + 5 * 4), &this_stamina_max, 4, &total); 
            read_success &= (total == 4);

            
            if (!read_success || health_max != this_health_max || stamina_max != this_stamina_max || mana_max != this_mana_max) {
                PCHARACTER = -1;
            } else {
                int x, y, z;
                int x_addr;

                bool chase_success = chase_pointer(&x_addr, handle, PCHARACTER, PCHARACTER_x_offsets, 2) + PCHARACTER_x_offsets[2];
                int x_addr_offset = x_addr - PCHARACTER;

                chase_success = chase_pointer(&x, handle, PCHARACTER, PCHARACTER_x_offsets, 3);
                chase_success &= chase_pointer(&y, handle, PCHARACTER, PCHARACTER_y_offsets, 3);
                chase_success &= chase_pointer(&z, handle, PCHARACTER, PCHARACTER_z_offsets, 3);

                if (!chase_success || 0 > x_addr_offset || x_addr_offset > 0xFFFFF) {
                    PCHARACTER = -1;
                }
            }
        }

        bool ended_search_early_because_time = false;

        // Search pages if couldn't reuse previous search result or searching for the first time
        if (PCHARACTER == -1) {
            clock_t search_start_time = clock();

            for (size_t i = 0; i < pages.size(); i++) {
                Page page = pages[i];

                // Sanity check before reading each page
                if (clock() - search_start_time > sanity_check_time) {
                    ended_search_early_because_time = true;
                    break;
                }

                DWORD total;
                if (!ReadProcessMemory(handle, page.start, page_buffer, page.size, &total) || total != page.size) {
                    printf("Failed to read process memory\n");
                    break;
                }

                // Search for PCHARACTER base by searching for health/mana/stamina range
                // NOTE: stop 20 bytes early to not get out of bounds
                for (size_t j = 0; j < total - 20; j += 4) {

                    // Sanity check while reading a page
                    if (j % inpage_sanity_check_interval == 0 && clock() - search_start_time > sanity_check_time) {
                        break;
                    }

                    // Confirm that we're at correct location by checking that
                    // 1. all stat maxes match the PSTATUS values
                    // 2. all stat maxes are equal or greater than current vals
                    // 3. the resulting x coordinate address is positive and no more than 0xFFFFF away from health adress
                    // NOTE: not sure if it's always 0xFFFFF
                    int this_health_max = read_bytes(page_buffer + j + 1 * 4, 4);
                    if (this_health_max != health_max) {
                        continue;
                    }

                    int this_mana_max = read_bytes(page_buffer + j + 3 * 4, 4);
                    if (this_mana_max != mana_max) {
                        continue;
                    }

                    int this_stamina_max = read_bytes(page_buffer + j + 5 * 4, 4);
                    if (this_stamina_max != stamina_max) {
                        continue;
                    }

                    int this_health = read_bytes(page_buffer + j + 0 * 4, 4);
                    int this_mana = read_bytes(page_buffer + j + 2 * 4, 4);
                    int this_stamina = read_bytes(page_buffer + j + 4 * 4, 4);

                    if (this_health > this_health_max || this_mana > this_mana_max || this_stamina > this_stamina_max) {
                        continue;
                    }

                    PCHARACTER = (int)page.start + j - PCHARACTER_stats_offset;
                    PCHARACTER_page = {
                        start: page.start,
                        size: page.size,
                    };

                    int x_addr;
                    bool chase_success = chase_pointer(&x_addr, handle, PCHARACTER, PCHARACTER_x_offsets, 2) + PCHARACTER_x_offsets[2];
                    int x_addr_offset = x_addr - ((int)page.start + j);
                    if (!chase_success || 0 > x_addr_offset || x_addr_offset > 0xFFFFF) {
                        PCHARACTER = -1;
                        continue;
                    }

                    break;
                }

                if (PCHARACTER != -1) {
                    break;
                }
            }

            // Save sanity check time after first successful search
            // NOTE: this is possibly what's causing loss of tracking, after death PCHARACTER is relocated and search takes longer than what is set even though the value is there
            if (need_to_get_sanity_check_time && PCHARACTER != -1) {
                need_to_get_sanity_check_time = false;
                sanity_check_time = 4 * (clock() - search_start_time);
            }

            // printf("search took %ld\n", clock() - search_start_time);
        }

        static clock_t last_write_success_time = 0;

        // Save coordinates to file
        if (PCHARACTER != -1) {
            int x, y, z;
            bool chase_success = chase_pointer(&x, handle, PCHARACTER, PCHARACTER_x_offsets, 3);
            chase_success &= chase_pointer(&y, handle, PCHARACTER, PCHARACTER_y_offsets, 3);
            chase_success &= chase_pointer(&z, handle, PCHARACTER, PCHARACTER_z_offsets, 3);

            if (chase_success) {
                if (print_debug) {
                    printf("Match at %x:\n", PCHARACTER);
                    printf("x=%f\ny=%f\nz=%f\n", hex_to_float(x), hex_to_float(y), hex_to_float(z));
                }

                // On death, the PCHARACTER struct is wiped and coordinates become very small, don't write those values
                float epsilon = 0.001;
                if (fabs(hex_to_float(x)) > epsilon && fabs(hex_to_float(z)) > epsilon && fabs(hex_to_float(y)) > epsilon) {
                    char buffer[100] = {0}; 
                    FILE* file = fopen(output_path, "a");

                    static clock_t prev_clock = clock();
                    clock_t current_clock = clock();
                    sprintf(buffer, "%ld,%x,%x,%x\n", current_clock - prev_clock, x, y, z);
                    prev_clock = current_clock;
                    fputs(buffer, file);

                    last_write_success_time = clock();

                    fclose(file);
                }
            }
        }

        // Sound alarm if no writes happen for too long
        if (clock() - last_write_success_time > no_writes_alarm_interval) {
            printf("\a");
            printf("Failed to read position. Restart recorder to continue.\n");
            // printf("Failed to read position. health_max=%d mana_max=%d stamina_max=%d, PCHARACTER=%d\n", health_max, mana_max, stamina_max, PCHARACTER);

            if (ended_search_early_because_time) {
                // printf("ended_search_early_because_time is SET\n");
            }

            if (PCHARACTER != -1) {
                int x, y, z;
                bool chase_success = chase_pointer(&x, handle, PCHARACTER, PCHARACTER_x_offsets, 3);
                chase_success &= chase_pointer(&y, handle, PCHARACTER, PCHARACTER_y_offsets, 3);
                chase_success &= chase_pointer(&z, handle, PCHARACTER, PCHARACTER_z_offsets, 3);

                if (chase_success) {
                    if (print_debug) {
                        // printf("Match at %x:\n", PCHARACTER);
                        // printf("x=%f\ny=%f\nz=%f\n", hex_to_float(x), hex_to_float(y), hex_to_float(z));
                    }
                } else {
                    // printf("chase failed\n");
                }
            }
        }

        CloseHandle(handle);
        Sleep(interval);
    }

    free(page_buffer);

    return 0;
}

