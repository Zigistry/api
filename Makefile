COMPILER = g++
BINARY_NAME = server
BUILD_FOLDER = build

SRC = src/main.cpp src/search.cpp src/scroll.cpp src/users.cpp src/packages_index_details.cpp src/programs_index_details.cpp src/helper_lib/helper_lib.cpp src/package_detial.cpp
TARGET = $(BUILD_FOLDER)/$(BINARY_NAME)
FLAGS =  -std=c++23 -DASIO_STANDALONE -Iinclude -lsqlite3

$(TARGET): $(SRC)
	mkdir -p $(BUILD_FOLDER)
	$(COMPILER) $(SRC) -o $(TARGET) $(FLAGS)

build: $(TARGET)
