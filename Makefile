CC = gcc
CXX = g++
CFLAGS = -Iinclude -I/opt/homebrew/opt/zstd/include -Wall -Wextra -std=c99 -Wno-deprecated-declarations
CXXFLAGS = -Iinclude -I/opt/homebrew/opt/zstd/include -Wall -Wextra -std=c++17 -fobjc-arc
LDFLAGS = -lz -L/opt/homebrew/opt/zstd/lib -lzstd

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Explicitly list objects
OBJECTS = $(OBJ_DIR)/filehandler.o \
          $(OBJ_DIR)/main.o \
          $(OBJ_DIR)/pichrono.o \
          $(OBJ_DIR)/utils.o \
          $(OBJ_DIR)/mongoose.o \
          $(OBJ_DIR)/webserver.o

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OBJECTS += $(OBJ_DIR)/piqme.o
    LDFLAGS += -framework Metal -framework Foundation
else
    OBJECTS += $(OBJ_DIR)/piqme_fallback.o
endif

EXECUTABLE = $(BIN_DIR)/pichrono

.PHONY: all clean run serve

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.mm | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Convenience target to build and run the Web UI
serve: all
	./$(EXECUTABLE) serve

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
