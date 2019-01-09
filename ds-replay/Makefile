# print variables for debugging
# $(info $$OBJ is [${OBJ}])

CC = gcc

SRC_DIR = src
OBJ_DIR = obj

EXE = main

SRC = $(wildcard $(SRC_DIR)/*.cpp)

OBJ += $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
OBJ += $(OBJ_DIR)/gl3w.o

CXXFLAGS = -I$(SRC_DIR)
CXXFLAGS += -I$(SRC_DIR)/libs/gl3w

LIBS = -lglfw3 -lgdi32 -lopengl32 -limm32

CXXFLAGS += -Wall -Wformat
CFLAGS = $(CXXFLAGS)

all: $(EXE)
	@echo Build complete

$(EXE): $(OBJ)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)
	
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/gl3w.o: $(SRC_DIR)/libs/gl3w/GL/gl3w.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXE) $(OBJ)
