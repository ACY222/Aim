CXX = clang++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20 -O2 -Iinclude

SRC_DIR = src
OBJ_DIR = build
TARGET = aim

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: $(TEST_TARGET)
	@echo "Running unit tests..."
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_DIR)/test_buffer.cpp $(OBJ_DIR)/buffer.o
	@echo "Compiling tests..."
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(TARGET) $(OBJ_DIR)

.PHONY: all clean test
