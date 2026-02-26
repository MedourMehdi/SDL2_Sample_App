# Makefile for SDL2 Atari ST validation test suite
# Targets: all (default), clean

CC = m68k-atari-mint-gcc
CFLAGS = -m68020-60 -O2
LIBS = -lSDL2 -lgem -lm

BUILD_DIR = builds

# Find all .c files in subdirectories (excluding the builds dir itself)
SRCS = $(wildcard */*.c)

# Determine which sources use SDL_mixer or SDL_ttf
MIXER_SRCS = $(shell grep -l 'SDL2/SDL_mixer.h' $(SRCS) 2>/dev/null)
TTF_SRCS   = $(shell grep -l 'SDL2/SDL_ttf.h'   $(SRCS) 2>/dev/null)

# Map each source to its target executable name:
#   - strip leading directory
#   - remove "test_" prefix
#   - replace .c with .prg
TARGETS = $(foreach src,$(SRCS),$(BUILD_DIR)/$(notdir $(patsubst test_%,%,$(src:.c=.prg))))

# Default target
all: $(TARGETS)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Pattern rule for compiling a single source file.
# The target is derived from the source name; the source is a prerequisite.
# After linking, set stack size and strip the binary.
define compile_c
$(BUILD_DIR)/$(notdir $(patsubst test_%,%,$(1:.c=.prg))): $(1) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $$@ $$< \
		$(if $(filter $(1),$(MIXER_SRCS)),-lSDL2_mixer -lSDL2 -lm -liconv -lgem -lxmp) \
		$(if $(filter $(1),$(TTF_SRCS)),-lSDL2_ttf -lfreetype -lz -lpng16 -lsdl2 -lm -liconv -lgem) \
		$(LIBS)
	m68k-atari-mint-stack --fix=128k $$@
	m68k-atari-mint-strip -s $$@
endef

# Generate a compilation rule for each source file
$(foreach src,$(SRCS),$(eval $(call compile_c,$(src))))

# Clean up build directory
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean