NAME = pdf
EXTENSION = .exe
OUTPUT = $(NAME)$(EXTENSION)
BUILD_DIR = build
OBJ_DIR = build/obj
JSON_FILE = temp.json
CC = clang 
CPP = clang++

SRC_DIR = src
SRC_1_DIR = src/platform
SRC_2_DIR = src/core
SRC_3_DIR = src/renderer
SRC_4_DIR = src/renderer/vulkan

INCLUDE_DIRS =-Isrc -IC:/Lib/SDL2/include -IC:/Lib/mupdf/include\
	-IC:/Lib/tracy/public -IC:/Lib/tracy/public/tracy

LIBS =-lSDL2main -lSDL2 -lSDL2_image -llibmupdf -llibthirdparty -lmsvcrtd -luser32\
	-lclang_rt.asan_dynamic_runtime_thunk-x86_64 -lclang_rt.asan_dynamic-x86_64

LIB_PATH =-LC:/Lib/SDL2/lib\
		  -LC:/Lib/mupdf-1.24.9-source/platform/win32/x64/Debug

CFLAGS = -MT -fsanitize=address -DTRACY_ENABLE
# CFLAGS = -MT -DTRACY_ENABLE -DTRACY_ENABLE

CFLAGS += -g -O0 -fdeclspec -MJ$(JSON_FILE)
CPPFLAGS = -MT -DTRACY_ENABLE -Wno-format

FILES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_1_DIR)/*.c)\
		$(wildcard $(SRC_2_DIR)/*.c) $(wildcard $(SRC_3_DIR)/*.c)\
		$(wildcard $(SRC_4_DIR)/*.c)

CPP_FILES = $(wildcard $(SRC_DIR)/*.cpp)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(FILES))
OBJS := $(patsubst $(OBJ_DIR)/platform/%.o, $(OBJ_DIR)/%.o, $(OBJS))
OBJS := $(patsubst $(OBJ_DIR)/core/%.o, $(OBJ_DIR)/%.o, $(OBJS))
OBJS := $(patsubst $(OBJ_DIR)/renderer/%.o, $(OBJ_DIR)/%.o, $(OBJS))
OBJS := $(patsubst $(OBJ_DIR)/vulkan/%.o, $(OBJ_DIR)/%.o, $(OBJS))

CPP_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(CPP_FILES))

all: $(OBJ_DIR) $(BUILD_DIR)/$(OUTPUT) compile_commands.json

$(BUILD_DIR)/$(OUTPUT): $(OBJS) $(CPP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(INCLUDE_DIRS) $(LIB_PATH) $(LIBS)
	@rm -f $(JSON_FILE)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	-$(CPP) $(CPPFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR)/%.o: $(SRC_1_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR)/%.o: $(SRC_2_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR)/%.o: $(SRC_3_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR)/%.o: $(SRC_4_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_DIRS)
	@-cat $(JSON_FILE) >> soon.json

$(OBJ_DIR): $(BUILD_DIR)
	@mkdir -p build/obj

$(BUILD_DIR):
	@mkdir -p build

compile_commands.json:
	@echo [ > compile_commands.json
	@-cat soon.json >> compile_commands.json
	@echo ] >> compile_commands.json
	@rm -f soon.json

clean:
	@echo Deleting files..
	rm -f $(BUILD_DIR)/$(NAME)$(EXTENSION)
	rm -f $(BUILD_DIR)/$(NAME).pdb
	rm -f $(BUILD_DIR)/$(NAME).exp
	rm -f $(BUILD_DIR)/$(NAME).lib
	rm -f $(BUILD_DIR)/vc140.pdb
	rm -f $(BUILD_DIR)/*.obj
	rm -f $(BUILD_DIR)/obj/*.o
	rm -f $(BUILD_DIR)/*.ilk

fclean: clean
	rm -f compile_commands.json

re: clean all

.PHONY: all clean re
