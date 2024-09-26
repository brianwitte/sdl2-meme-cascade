CFLAGS = -Wall -Wextra -O2 -std=c99 -g2 `sdl2-config --cflags`
CFLAGS += -Wno-unused-function -I. -I$(SRC_DIR) -I$(LIB_DIR)

LDFLAGS = `sdl2-config --libs` -lm -lSDL2_ttf

SRC_DIR = .
BIN_DIR = bin
LIB_DIR = libs

SRC_FILES = $(SRC_DIR)/meme_cascade.c $(SRC_DIR)/stb_vorbis.c $(SRC_DIR)/SDL_FontCache.c
LIB_HEADERS =

BIN_NAME = meme_cascade
BIN_PATH = $(BIN_DIR)/$(BIN_NAME)

all: $(BIN_DIR) $(BIN_PATH)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(BIN_PATH): $(SRC_FILES) $(LIB_HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC_FILES) $(LDFLAGS)

clean:
	@rm -rf $(BIN_DIR)

zen:
	@echo "Makefile Zen:"
	@echo "1. Simplicity: Explicit rules, no wildcards."
	@echo "2. Portability: No OS-specific assumptions."
	@echo "3. Minimalism: Essential flags and rules only."
	@echo "4. Modularity: Clean separation of targets."
	@echo "5. Transparency: No hidden logic."
	@echo "6. Feedback: Clear output for each step."

help:
	@echo "Usage: make [TARGET]"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the game binary."
	@echo "  clean     - Clean the build artifacts."
	@echo "  zen       - Display the Makefile Zen principles."
	@echo "  help      - Display this help message."

.PHONY: all clean zen help
