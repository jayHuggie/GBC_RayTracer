#
# Makefile for GBC Raytracer
#
# Builds a Game Boy Color ROM that displays a simple raytraced scene.
# Requires GBDK-2020 (https://github.com/gbdk-2020/gbdk-2020)
#
# Usage:
#   make          - Build the ROM
#   make clean    - Remove build artifacts
#
# Dependencies:
#   - GBDK-2020 installed (set GBDK_HOME if not in ~/gbdk/)
#   - make, standard Unix tools
#

# GBDK installation directory
# Override with: make GBDK_HOME=/path/to/gbdk/
ifndef GBDK_HOME
	GBDK_HOME = ~/gbdk/
endif

# GBDK compiler wrapper
LCC = $(GBDK_HOME)bin/lcc

# Compiler flags:
#   -Wm-yC : Generate Game Boy Color ROM (not regular GB)
#   -Wm-yn"RAYTRACER" : Set ROM name in header
LCCFLAGS = -Wm-yC -Wm-yn"RAYTRACER"

# Enable debug mode if requested
# Usage: make GBDK_DEBUG=ON
ifdef GBDK_DEBUG
	LCCFLAGS += -debug -v
endif

# Output ROM name
PROJECTNAME = GBC_RayTracer

# Build targets
BINS = $(PROJECTNAME).gbc

# Source files
CSOURCES := $(wildcard src/*.c)

# Default target
all: $(BINS)

# Build the ROM
# GBDK's lcc handles compiling and linking in one step
$(BINS): $(CSOURCES)
	$(LCC) $(LCCFLAGS) -o $@ $(CSOURCES)
	@echo "========================================="
	@echo "Build complete: $@"
	@echo "ROM size: $$(wc -c < $@) bytes"
	@echo "========================================="

# Clean build artifacts
clean:
	rm -f *.o *.lst *.map *.gbc *.gb *.ihx *.sym *.cdb *.adb *.asm *.noi *.rst

# Phony targets
.PHONY: all clean

# Generate compile.bat for Windows users
compile.bat: Makefile
	@echo "REM Automatically generated from Makefile" > compile.bat
	@make -sn | sed y/\\//\\\\/ | sed s/mkdir\ \-p/mkdir/ | grep -v make >> compile.bat
