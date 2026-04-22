CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99 -Wno-deprecated-declarations
LDFLAGS = -lz

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Explicitly list objects to ensure we track all components
OBJECTS = $(OBJ_DIR)/filehandler.o \
          $(OBJ_DIR)/main.o \
          $(OBJ_DIR)/pichrono.o \
          $(OBJ_DIR)/utils.o \
          $(OBJ_DIR)/mongoose.o \
          $(OBJ_DIR)/webserver.o

EXECUTABLE = $(BIN_DIR)/pichrono

.PHONY: all clean run serve

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Convenience target to build and run the Web UI
serve: all
	./$(EXECUTABLE) serve

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
