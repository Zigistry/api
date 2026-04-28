COMPILER = zig c++
BINARY_NAME = server
BUILD_FOLDER = build

SRC = src/main.cpp

build:
	mkdir -p $(BUILD_FOLDER)
	$(COMPILER) $(SRC) -o $(BUILD_FOLDER)/$(BINARY_NAME) 
