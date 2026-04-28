COMPILER = zig c++
BINARY_NAME = server
BUILD_FOLDER = build

SRC = src/main.cpp
FLAGS = -Linclude -lsql


build:
	mkdir -p $(BUILD_FOLDER)
	$(COMPILER) $(SRC) -o $(BUILD_FOLDER)/$(BINARY_NAME) $(FLAGS)
