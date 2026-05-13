COMPILER = zig c++
BINARY_NAME = server
BUILD_FOLDER = build

SRC = src/main.cpp src/search_packages.cpp
TARGET = $(BUILD_FOLDER)/$(BINARY_NAME)
FLAGS = -Linclude -lsql -std=c++23

$(TARGET): $(SRC)
	mkdir -p $(BUILD_FOLDER)
	$(COMPILER) $(SRC) -o $(TARGET) $(FLAGS)

build: $(TARGET)
