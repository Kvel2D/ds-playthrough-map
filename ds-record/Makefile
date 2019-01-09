CC = gcc

SRC_DIR = src
OBJ_DIR = obj

EXE = main

SRC = $(wildcard $(SRC_DIR)/*.cpp)

OBJ += $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

CXXFLAGS = -I$(SRC_DIR)
CXXFLAGS += -Wall -Wformat

LIBS = 


all: $(EXE)
	@echo Build complete

$(EXE): $(OBJ)
	@mkdir -p $(@D)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)
	
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(EXE) $(OBJ)
