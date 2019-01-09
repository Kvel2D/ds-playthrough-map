#include <stdio.h>
#include <math.h>
#include <fstream>
#include <string>
#include <vector>

#include <GL/gl3w.h> 
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/normal.hpp>
#include <glm/gtc/type_ptr.hpp>

#define print(a, args...) printf("%s(%s:%d) " a,  __func__,__FILE__, __LINE__, ##args)
#define trace(a, args...) print(a "\n", ##args)
#define array_size(_array) (sizeof(_array)/sizeof(*_array)) 
#define rgb_to_vec3(r, g, b) glm::vec3((float)r / 255, (float)g / 255, (float)b / 255) 

struct Model {
    GLuint offset;
    GLuint vertex_count;
};

// TODO: add imgui
// add buttons for "lerp to marker position", "pause/play", "rewind to start", "rewind to end", "chance playback speed", changing the amount of markers drawn(drawing 3k would mean drawing starting from current - 3k index)

const char* zone_model_paths[] = {
    "darksoulscollisionmap/18-1 Undead Asylum.obj",
    "darksoulscollisionmap/10-2 Firelink Shrine.obj",
    "darksoulscollisionmap/10-1 Undead Burg.obj",
    "darksoulscollisionmap/10-0 Depths.obj",
    "darksoulscollisionmap/14-0 Blighttown+Quelaags Domain.obj",
    "darksoulscollisionmap/13-2 Ash Lake.obj",
    "darksoulscollisionmap/12-0 Darkroot Garden+Basin.obj",
    "darksoulscollisionmap/13-0 Catacombs.obj",
    "darksoulscollisionmap/15-0 Sens Fortress.obj",
    "darksoulscollisionmap/15-1 Anor Londo.obj",
    "darksoulscollisionmap/11-0 Painted World of Ariamis.obj",
    "darksoulscollisionmap/17-0 Duke\'s Archive+Crystal Caves.obj",
    "darksoulscollisionmap/13-1 Tomb of the Giants.obj",
    "darksoulscollisionmap/14-1 Demon Ruins+Lost Izalith.obj",
    "darksoulscollisionmap/16-0 New Londo Ruins+Valley of Drakes.obj",
    "darksoulscollisionmap/18-0 Kiln of the first Flame.obj",
    "darksoulscollisionmap/12-1 Oolacile.obj",
};

const char* marker_model_path = "sphere.obj"; 

const glm::vec3 zone_colors[] = {
    rgb_to_vec3(68, 137, 26), // asylum
    rgb_to_vec3(100, 171, 137), // firelink
    rgb_to_vec3(155, 191, 155), // undead burg
    rgb_to_vec3(105, 145, 147), // depths
    rgb_to_vec3(147, 141, 117), // blighttown
    rgb_to_vec3(121, 220, 217), // ash lake
    rgb_to_vec3(71, 163, 140), // darkroot
    rgb_to_vec3(79, 99, 108), // catacombs
    rgb_to_vec3(132, 114, 92), // sen's
    rgb_to_vec3(222, 208, 163), // anor lond
    rgb_to_vec3(191, 207, 196), // painted world
    rgb_to_vec3(163, 224, 208), // duke's
    rgb_to_vec3(64, 83, 89), // tomb of giants
    rgb_to_vec3(242, 123, 81), // izalith
    rgb_to_vec3(68, 105, 111), // new londo
    rgb_to_vec3(167, 195, 198), // kiln
    rgb_to_vec3(71, 163, 140), // oolacile
};
glm::vec3 nice_red = rgb_to_vec3(190, 38, 15);
glm::vec3 nice_blue = rgb_to_vec3(49, 162, 242);

// const int default_screen_width = 1280;
// const int default_screen_height = 720;
const int default_screen_width = 1920;
const int default_screen_height = 1200;

GLFWwindow* window; 

const float camera_move_speed = 1.0f * 60.0f;
const float camera_pan_speed = 0.040f * 60.0f;
glm::vec3 camera_position(-50.0f, 330.0f, 50.0f);
glm::quat camera_orientation(glm::vec3(glm::radians(60.0f), 0.0f, 0.0f));
glm::mat4 camera_view;

const GLuint buffer_count = 2;
GLuint buffers[buffer_count];

GLint projection_location;
GLint position_location;
GLint normal_location;
GLint offset_location;
GLint model_location;
GLint view_location;
GLint light_pos_location;
GLint light_color_location;
GLint object_color_location;

std::vector<Model> zone_models;
Model marker_model;

const char* position_history_path = "positions.txt";
const float replay_speed = 100.0f;
std::vector<float> delta_history;
size_t position_history_size;
size_t position_history_ptr = 0;
glm::vec3 first_offset;
std::vector<glm::vec3> position_history;

const float blue_marker_scale = 0.75f;
const float red_marker_scale = 2.0f;

bool skip_stationary = true;

float hex_to_float(int x) {
    return *((float*)&x);
}

void print_program_log(GLuint program) {
    if (!glIsProgram(program)) {
        trace("Name %d is not a program", program);
        return;
    }

    int info_log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);

    char *info_log = new char[info_log_length];

    int read_length;
    glGetProgramInfoLog(program, info_log_length, &read_length, info_log);

    if (read_length > 0) {
        trace("%s", info_log);
    }

    delete[] info_log;
}

void print_shader_log(GLuint shader) {
    if (!glIsShader(shader)) {
        trace("Name %d is not a shader", shader);
        return;
    }

    // Get info log length
    int info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);

    char* info_log = new char[info_log_length];

    // Get info log
    int read_length;
    glGetShaderInfoLog(shader, info_log_length, &read_length, info_log);

    if (read_length > 0) {
        trace("%s", info_log);
    }

    delete[] info_log;
}

GLuint load_shader_file(const char* shader_path, GLenum shader_type) {
    std::ifstream shader_file(shader_path);

    if (!shader_file) {
        trace("Failed to open file %s", shader_path);
        return 0;
    }

    std::string shader_string;

    shader_string.assign((std::istreambuf_iterator<char>(shader_file)),
        std::istreambuf_iterator<char>());

    GLuint shader_id = glCreateShader(shader_type);
    const GLchar *shaderSource = shader_string.c_str();
    glShaderSource(shader_id, 1, &shaderSource, NULL);
    glCompileShader(shader_id);
    GLint shader_compiled = GL_FALSE;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &shader_compiled);

    if (shader_compiled != GL_TRUE) {
        trace("Failed to compile shader %d!\n\nSource:\n%s", shader_id, shader_string.c_str());
        print_shader_log(shader_id);
        glDeleteShader(shader_id);
        return 0;
    }

    return shader_id;
}

GLfloat* load_model(GLuint* vertex_count, const char* path, bool invert_downward_normals) {
    char bin_path[200];
    strcpy(bin_path, path);
    char* extension = strstr(bin_path, ".obj");
    strcpy(extension, ".bin");

    GLfloat* vertex_buffer;

    // Try opening pre-parsed bin file
    FILE* bin_file_in = fopen(bin_path, "rb");
    if (bin_file_in != NULL) {
        fread(vertex_count, sizeof(GLuint), 1, bin_file_in);
        vertex_buffer = (GLfloat*)malloc(*vertex_count * 6 * sizeof(GLfloat));
        fread(vertex_buffer, sizeof(GLfloat), *vertex_count * 6, bin_file_in);
        fclose(bin_file_in);

        return vertex_buffer;
    }

    // Parse normally if no bin file found
    FILE* file = fopen(path, "r");

    if (file == NULL) {
        printf("failed to open model file %s\n", path);
        return NULL;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;
    char line_header[100];

    while (true) {
        int scan_result = fscanf(file, "%s", line_header);
        if (scan_result == EOF) {
            break;
        }

        if (strcmp(line_header, "v") == 0) {
            GLfloat vertex[3];
            fscanf(file, "%f %f %f\n", &vertex[0], &vertex[1], &vertex[2]);
            for (size_t i = 0; i < 3; i++) {
                vertices.push_back(vertex[i]);
            }
        } else if (strcmp(line_header, "f") == 0) {
            GLuint index[3];
            fscanf(file, "%d %d %d\n", &index[0], &index[1], &index[2]);
            for (size_t i = 0; i < 3; i++) {
                indices.push_back(index[i]);
            }
        }
    }

    *vertex_count = indices.size();
    vertex_buffer = (GLfloat*)malloc(*vertex_count * 6 * sizeof(GLfloat));
    glm::vec3 p[3];
    for (size_t i = 0; i < indices.size(); i += 3) {
        // Save 3 vertices to buffer and temp vertices for normal calculation
        for (size_t j = 0; j < 3; j++) {

            size_t v_i = indices[i + j] - 1;
            for (size_t k = 0; k < 3; k++) {
                GLfloat component = vertices[v_i * 3 + k];
                vertex_buffer[(i + j) * 6 + k] = component;
                (&p[j][0])[k] = component;
            }
        }

        glm::vec3 normal = glm::normalize(glm::triangleNormal(p[0], p[1], p[2]));

        // Reflect downward normals, makes it look better from above
        if (invert_downward_normals && normal.y < 0) {
            normal = -normal;
        }

        // Save normals to buffer
        for (size_t j = 0; j < 3; j++) {
            for (size_t k = 0; k < 3; k++) {
                vertex_buffer[(i + j) * 6 + 3 + k] = (&normal[0])[k];
            }
        }
    }

    // Save parsed bin file
    FILE* bin_file_out = fopen(bin_path, "wb");
    fwrite(vertex_count, sizeof(GLuint), 1, bin_file_out);
    fwrite(vertex_buffer, sizeof(GLfloat), *vertex_count * 6, bin_file_out);
    fclose(bin_file_out);

    return vertex_buffer;
}

bool playing = false;

void update() {
    static float prev_time = 0.0f;
    float delta_time = (float)glfwGetTime() - prev_time;
    prev_time = (float)glfwGetTime();

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) playing = true;
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) playing = false;

    // Clear screen buffers
    static const GLfloat color[] = {0.5f, 0.5f, 0.5f, 1.0f};
    glClearBufferfv(GL_COLOR, 0, color);
    static const GLfloat depth = 1.0f;
    glClearBufferfv(GL_DEPTH, 0, &depth);

    // Move camera towards last marker
    // static bool lerping_position = false;
    // glm::vec3 marker_pos = position_history[position_history_ptr];
    // if (!lerping_position && glm::distance(marker_pos, camera_position) > 200.0f) {
    //     lerping_position = true;
    // }

    // if (lerping_position) {
    //     float t = 0.01f;
    //     camera_position = marker_pos * t + camera_position * (1.0f - t);

    //     if (glm::distance(marker_pos, camera_position) < 150.0f) {
    //         lerping_position = false;
    //     }
    // }

    // glm::vec3 angles = glm::eulerAngles(q);

    // static bool lerping_orientation = false;
    // glm::quat marker_angles = glm::eulerAngles(glm::quat(glm::vec4(marker_pos - camera_position, 1.0f)));
    // if (glm::distance(marker_pos, camera_position) < 0.1f) {
    //     lerping_orientation = false;
    // }
    // if (!lerping_orientation && marker_angles.y - angles.y > 0.2f) {
    //     lerping_orientation = true;
    // }

    // if (lerping_orientation) {
    //     float t = 0.01f;
    //     camera_orientation = glm::lerp(camera_orientation, marker_orientation, t);
    // }
    

    //
    // Update camera
    //

    static float prev_mouse_x; 
    static float prev_mouse_y;
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    float mouse_dx = (float)mouse_x - prev_mouse_x;
    float mouse_dy = (float)mouse_y - prev_mouse_y;
    prev_mouse_x = (float)mouse_x;
    prev_mouse_y = (float)mouse_y;

    glm::vec3 camera_pan(0.0f);
    float sensitivity = 0.1f;
    camera_pan.x += mouse_dy * sensitivity;
    camera_pan.y += mouse_dx * sensitivity;

    // Update camera orientation
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) camera_pan.x += -camera_pan_speed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) camera_pan.x += camera_pan_speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) camera_pan.y += -camera_pan_speed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camera_pan.y += camera_pan_speed;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) camera_pan.z += -camera_pan_speed;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) camera_pan.z += camera_pan_speed;
    camera_pan *= delta_time;
    // NOTE: Rotate around world y-axis instead of camera's y-axis to remove roll
    camera_orientation = glm::quat(glm::vec3(camera_pan.x, 0.0f, camera_pan.z)) * camera_orientation;
    camera_orientation = glm::rotate(camera_orientation, camera_pan.y, glm::vec3(0.0f, 1.0f, 0.0f));
    camera_orientation = glm::normalize(camera_orientation);

    // Update camera position
    glm::vec3 camera_move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera_move.x += 1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera_move.x += -1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera_move.z += 1;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera_move.z += -1;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camera_move.y += 1; 
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera_move.y += -1;

    glm::vec3 forward(camera_view[0][2], camera_view[1][2], camera_view[2][2]);
    glm::vec3 strafe(camera_view[0][0], camera_view[1][0], camera_view[2][0]);
    // NOTE: Move along world y-axis instead of camera's y-axis
    glm::vec3 up_down(0.0f, 1.0f, 0.0f);

    camera_position += (camera_move.z * forward + camera_move.x * strafe + camera_move.y * up_down) * camera_move_speed * delta_time;

    // Update cameraview
    glm::mat4 camera_rotation = glm::mat4_cast(camera_orientation);
    glm::mat4 camera_translation = glm::translate(glm::mat4(1.0f), -camera_position);
    camera_view = camera_rotation * camera_translation;
    glUniformMatrix4fv(view_location, 1, GL_FALSE, glm::value_ptr(camera_view));

    //
    // Position history markers
    //

    // Increment position history pointer
    if (playing && position_history_ptr < position_history_size - 1) {
        static float delta_time_buffer = 0.0f;
        delta_time_buffer += delta_time * replay_speed;

        while (delta_time_buffer > 0.0f && position_history_ptr < position_history_size - 1) {
            delta_time_buffer -= delta_history[position_history_ptr];
            position_history_ptr++;
        }
    }

    glUniformMatrix4fv(model_location, 1, GL_FALSE, glm::value_ptr(glm::scale(glm::vec3(blue_marker_scale))));

    // Draw all except last marker as blue
    if (position_history_ptr > 1) {
        glUniform4f(object_color_location, nice_blue.x, nice_blue.y, nice_blue.z, 1.0f);
        glDrawArraysInstancedBaseInstance(GL_TRIANGLES, marker_model.offset, marker_model.vertex_count, (position_history_ptr - 1) + 1, 0);
    }

    // Draw last marker as red
    glDisable(GL_DEPTH_TEST);
    glUniform4f(object_color_location, nice_red.x, nice_red.y, nice_red.z, 1.0f);
    // NOTE: Make red marker bigger so it's easier to see
    glUniformMatrix4fv(model_location, 1, GL_FALSE, glm::value_ptr(glm::scale(glm::vec3(red_marker_scale))));
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, marker_model.offset, marker_model.vertex_count, 1, position_history_ptr);
    glEnable(GL_DEPTH_TEST);

    //
    // Zone models
    //

    // Set offset attrib to zero for zone models
    GLfloat zero_offset[3] = {0.0f, 0.0f, 0.0f};
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(GLfloat), (GLfloat*)zero_offset);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUniformMatrix4fv(model_location, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

    // Draw zone models
    for (size_t i = 0; i < zone_models.size() - 1; i++) {
        Model model = zone_models[i];

        glm::vec3 color = zone_colors[i];
        glUniform4f(object_color_location, color.x, color.y, color.z, 0.5f);

        glDrawArrays(GL_TRIANGLES, model.offset, model.vertex_count);
    }

    // Restore first offset
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * sizeof(GLfloat), glm::value_ptr(first_offset));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Print camera position
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) { 
        printf("camera_position=(%f, %f, %f)\n", camera_position.x, camera_position.y, camera_position.z);
    }
}

void window_size_callback(GLFWwindow* window, int width, int height) {
    glm::mat4 projection = glm::perspective(glm::radians(51.0f), (float)width / (float)height, 0.1f, 10000.0f);
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, glm::value_ptr(projection));
    glViewport(0, 0, width, height);
}

void glfw_error_callback(int error, const char *description) {
    trace("GLFW Error %d: %s", error, description);
}

int main(int, char**) {
    //
    // Init glfw and gl3w
    //
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == 0) {
        return 1;
    }

    window = glfwCreateWindow(default_screen_width, default_screen_height, "main", glfwGetPrimaryMonitor(), NULL);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // window = glfwCreateWindow(default_screen_width, default_screen_height, "main", NULL, NULL);
    if (window == NULL) {
        trace("Failed to create glfw window");
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowSizeCallback(window, window_size_callback);

    if (gl3wInit() != 0) {
        trace("Failed to init GL3W");
        return 1;
    }

    //
    // Load and link shaders
    //
    GLuint vertex_shader = load_shader_file("src/vertex.glsl", GL_VERTEX_SHADER);
    GLuint fragment_shader = load_shader_file("src/fragment.glsl", GL_FRAGMENT_SHADER);
    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    // Compile program and check for program compilation errors
    GLint program_compiled;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &program_compiled);
    if (program_compiled != GL_TRUE) {
        trace("Error linking program %d", shader_program);
        print_program_log(shader_program);
        glDeleteProgram(shader_program);
        shader_program = 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glUseProgram(shader_program);

    // Get attrib and uniform locations
    position_location = glGetAttribLocation(shader_program, "position_in");
    normal_location = glGetAttribLocation(shader_program, "normal_in");
    offset_location = glGetAttribLocation(shader_program, "offset_in");
    projection_location = glGetUniformLocation(shader_program, "projection");
    model_location = glGetUniformLocation(shader_program, "model");
    view_location = glGetUniformLocation(shader_program, "view");
    light_pos_location = glGetUniformLocation(shader_program, "light_pos");
    light_color_location = glGetUniformLocation(shader_program, "light_color");
    object_color_location = glGetUniformLocation(shader_program, "object_color");

    glUniform3f(light_color_location, 1.0f, 1.0f, 1.0f);
    glUniform3f(light_pos_location, 0.0f, 1000.0f, 0.0f);

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Call window size callback with default dimensions to initialize projection and viewport 
    window_size_callback(window, default_screen_width, default_screen_height);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //
    // VBO's
    //
    glGenBuffers(buffer_count, buffers);

    //
    // Vertex positions and normals
    //
    GLuint vbo_vertex_count = 0;
    std::vector<GLfloat*> zone_model_buffers;

    // Load models
    for (size_t i = 0; i < array_size(zone_model_paths); i++) {
        Model model;
        GLfloat* vertex_buffer = load_model(&model.vertex_count, zone_model_paths[i], true);
        zone_model_buffers.push_back(vertex_buffer);
        model.offset = vbo_vertex_count;
        vbo_vertex_count += model.vertex_count;
        zone_models.push_back(model);
    }

    GLfloat* marker_vertex_buffer = load_model(&marker_model.vertex_count, marker_model_path, false);
    marker_model.offset = vbo_vertex_count;
    vbo_vertex_count += marker_model.vertex_count;

    // Fill vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vbo_vertex_count * 6 * sizeof(GLfloat), NULL, GL_STATIC_DRAW);

    for (size_t i = 0; i < zone_models.size(); i++) {
        Model model = zone_models[i];
        GLfloat* buffer = zone_model_buffers[i];
        glBufferSubData(GL_ARRAY_BUFFER, model.offset * 6 * sizeof(GLfloat), model.vertex_count * 6 * sizeof(GLfloat), buffer);
        free(buffer);
    }
    glBufferSubData(GL_ARRAY_BUFFER, marker_model.offset * 6 * sizeof(GLfloat), marker_model.vertex_count * 6 * sizeof(GLfloat), marker_vertex_buffer);
    free(marker_vertex_buffer);

    glVertexAttribPointer(position_location, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(position_location);

    glVertexAttribPointer(normal_location, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(normal_location);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    //
    // Instance offsets
    //

    // Load position history
    GLfloat* offsets;
    {   
        const char* path = position_history_path;
        FILE* file = fopen(path, "r");
        if (file == NULL) {
            printf("Failed to open position_history file %s\n", path);
            exit(1);
        }

        int x, y, z;
        int prev_x, prev_y, prev_z;
        int delta;
        while (true) {
            int result = fscanf(file, "%d,%x,%x,%x\n", &delta, &x, &y, &z);

            if (result == -1) {
                break;
            }

            if (skip_stationary && prev_x == x && prev_y == y && prev_z == z) {
                continue;
            }

            prev_x = x;
            prev_y = y;
            prev_z = z;

            // NOTE: need to swap x and z axes
            position_history.push_back(glm::vec3(hex_to_float(z), hex_to_float(y) + 1.0f, hex_to_float(x)));

            delta_history.push_back((float)delta / 1000);
        }
        fclose(file);

        position_history_size = position_history.size();
        first_offset = position_history[0];

        // Move offsets from vector into an array
        offsets = (GLfloat*)malloc(position_history_size * 3 * sizeof(GLfloat));
        for (size_t i = 0; i < position_history_size; i++) {
            glm::vec3 pos = position_history[i];
            for (size_t j = 0; j < 3; j++) {
                offsets[i * 3 + j] = (&pos[0])[j];
            }
        }
    }

    // Fill instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, position_history_size * 3 * sizeof(GLfloat), offsets, GL_DYNAMIC_DRAW);
    free(offsets);
    glEnableVertexAttribArray(offset_location);
    glVertexAttribPointer(offset_location, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glVertexAttribDivisor(offset_location, 1); 
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    while (!glfwWindowShouldClose(window)) {
        double before = glfwGetTime();
        glfwPollEvents();
        glfwMakeContextCurrent(window);


        update();


        glfwSwapBuffers(window);
        // printf("frametime=%f\n", glfwGetTime() - before);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(buffer_count, buffers);
    glDeleteProgram(shader_program);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

