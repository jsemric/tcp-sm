CXX=g++
CXXFLAGS=-Wall -Wextra -std=c++20 -Werror=return-type -I./src

DEV_ONLY ?= 0

ifeq ($(DEV_ONLY), 1)
	CXXFLAGS+=-O0 -g
else
	CXXFLAGS+=-O2
endif

SRC_DIR=src
TARGET_NAME = tcp-sm
BUILD_DIR = build
TEST_DIR = test

all: build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

build: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking executable: $(TARGET)"
	$(CXX) $^ -o $@ $(CXXFLAGS)
	@echo "Compilation complete."

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

TEST_NAME=test/run_test
GTEST_DIR = googletest/googletest
GTEST_SRC = $(GTEST_DIR)/src/gtest-all.cc
GTEST_LIB_PATH = lib
GTEST_CXXFLAGS = -isystem $(GTEST_DIR)/include -isystem $(GTEST_DIR) -pthread
GTEST_TEST_SRC = $(TEST_DIR)/unittests.cpp

$(GTEST_LIB_PATH)/libgtest.a: $(GTEST_SRC)
	@mkdir -p $(GTEST_LIB_PATH)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) -c $(GTEST_SRC) -o $(GTEST_LIB_PATH)/gtest-all.o
	$(AR) rcs $(GTEST_LIB_PATH)/libgtest.a $(GTEST_LIB_PATH)/gtest-all.o
	rm -f $(GTEST_LIB_PATH)/gtest-all.o

test: $(GTEST_LIB_PATH)/libgtest.a $(TEST_NAME)
	./$(TEST_NAME)

test-repeat: $(GTEST_LIB_PATH)/libgtest.a $(TEST_NAME)
	./$(TEST_NAME) --gtest_repeat=20

$(TEST_NAME): $(filter-out $(BUILD_DIR)/main.o, $(OBJS)) $(GTEST_LIB) $(GTEST_TEST_SRC)
	$(CXX) $(CXXFLAGS) $(GTEST_CXXFLAGS) $^ -o $@ -L$(GTEST_LIB_PATH) -lgtest -pthread

.PHONY: all clean build test

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR) $(GTEST_LIB_PATH) $(TEST_NAME)
	@echo "Cleanup complete."
