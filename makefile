CXX      := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Isrc/include

SRC_DIR   := src
LIB_DIR   := $(SRC_DIR)/lib
TEST_DIR  := tests
OBJ_DIR   := obj
BIN_DIR   := bin

TARGET      := $(BIN_DIR)/out
TEST_TARGET := $(BIN_DIR)/test_runner

# Main app sources (excluding tests)
APP_SRCS := $(SRC_DIR)/main.cpp $(shell find $(LIB_DIR) -name '*.cpp' 2>/dev/null)
APP_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(APP_SRCS))

# Lib sources needed for tests
LIB_SRCS := $(shell find $(LIB_DIR) -name '*.cpp' 2>/dev/null)
LIB_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(LIB_SRCS))

# Test sources
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.cpp' 2>/dev/null)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(APP_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(APP_OBJS) -o $@

$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LIB_OBJS) $(TEST_OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	@echo "\n=== Running Unit Tests ==="
	@./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
