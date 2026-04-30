COMPILER = zig c++
BINARY_NAME = server
BUILD_FOLDER = build

SRC = src/main.cpp
TARGET = $(BUILD_FOLDER)/$(BINARY_NAME)
FLAGS = -Linclude -lsql

$(TARGET): $(SRC)
	mkdir -p $(BUILD_FOLDER)
	$(COMPILER) $(SRC) -o $(TARGET) $(FLAGS)

build: $(TARGET)
