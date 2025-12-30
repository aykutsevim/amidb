# Cross-Compiling for Amiga on Debian Linux

A comprehensive guide for setting up a modern cross-compilation environment to build software for classic Amiga systems (68000-based) on Debian/Ubuntu Linux.

## Table of Contents

1. [Introduction](#introduction)
2. [Prerequisites](#prerequisites)
3. [Installing m68k-amigaos-gcc](#installing-m68k-amigaos-gcc)
4. [Testing Your Toolchain](#testing-your-toolchain)
5. [Setting Up Amiberry](#setting-up-amiberry)
6. [Development Workflow](#development-workflow)
7. [Debugging Tips](#debugging-tips)
8. [Troubleshooting](#troubleshooting)

---

## Introduction

### Why Cross-Compile?

Developing directly on vintage Amiga hardware is charming but impractical for serious development:

- **Slow compilation**: A 7MHz 68000 takes minutes to compile what your modern PC does in seconds
- **Limited RAM**: 2MB (or even 512KB) makes large projects impossible to build natively
- **No modern tools**: No git, no modern editors, no CI/CD pipelines
- **Hardware fragility**: Vintage hardware fails; your development shouldn't depend on it

Cross-compilation gives you the best of both worlds: modern development tools with authentic Amiga binaries.

### What We're Building

This guide sets up:

1. **m68k-amigaos-gcc** - A modern GCC toolchain (6.5.0) that produces Amiga executables
2. **Amiberry** - A fast, accurate Amiga emulator for testing
3. **A shared folder** - Seamless file transfer between host and emulated Amiga

---

## Prerequisites

### System Requirements

- **OS**: Debian 11+, Ubuntu 20.04+, Raspberry Pi OS, or similar
- **Disk Space**: ~2GB for toolchain, ~500MB for Amiberry
- **RAM**: 4GB minimum (8GB recommended for comfortable development)

### Required Packages

Install essential build tools:

```bash
# Update package lists
sudo apt update

# Install build essentials
sudo apt install -y \
    build-essential \
    git \
    wget \
    curl \
    lhasa \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    flex \
    bison \
    texinfo

# For Amiberry (SDL2-based emulator)
sudo apt install -y \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    libpng-dev \
    zlib1g-dev \
    libflac-dev \
    libmpeg2-4-dev \
    libserialport-dev
```

---

## Installing m68k-amigaos-gcc

### Option A: Pre-built Binaries (Recommended)

The AmigaPorts project provides pre-built toolchains. This is the fastest method.

#### For x86_64 Linux:

```bash
# Create installation directory
sudo mkdir -p /opt/amiga
cd /opt/amiga

# Download latest release (check GitHub for current version)
wget https://github.com/AmigaPorts/m68k-amigaos-gcc/releases/download/GCC6.5.0b/gcc-6.5.0b-x86_64-linux.tar.bz2

# Extract
sudo tar xjf gcc-6.5.0b-x86_64-linux.tar.bz2

# Clean up
sudo rm gcc-6.5.0b-x86_64-linux.tar.bz2
```

#### For Raspberry Pi (ARM):

```bash
# Pre-built ARM binaries may not be available
# You'll need to build from source (see Option B)
# Or use a Docker container with x86_64 emulation
```

#### Add to PATH

Add the toolchain to your shell configuration:

```bash
# Add to ~/.bashrc or ~/.zshrc
echo 'export PATH="/opt/amiga/m68k-amigaos/bin:$PATH"' >> ~/.bashrc

# Reload shell configuration
source ~/.bashrc

# Verify installation
m68k-amigaos-gcc --version
```

Expected output:
```
m68k-amigaos-gcc (GCC) 6.5.0
Copyright (C) 2017 Free Software Foundation, Inc.
```

### Option B: Building from Source

If pre-built binaries aren't available for your architecture:

```bash
# Clone the repository
git clone https://github.com/AmigaPorts/m68k-amigaos-gcc.git
cd m68k-amigaos-gcc

# Review the build script (important!)
less build.sh

# Build (this takes 1-3 hours depending on your system)
./build.sh

# The toolchain will be installed to ~/m68k-amigaos by default
# Add to PATH as shown above
```

#### Build Options

You can customize the build:

```bash
# Build for a specific CPU (default is 68000)
./build.sh --with-cpu=68020

# Specify installation prefix
./build.sh --prefix=/opt/amiga/m68k-amigaos

# Build with debug symbols (larger binaries, useful for toolchain debugging)
./build.sh --enable-debug
```

---

## Testing Your Toolchain

### Hello World Test

Create a simple test program:

```bash
mkdir -p ~/amiga-test
cd ~/amiga-test
```

Create `hello.c`:

```c
/*
 * hello.c - Minimal Amiga test program
 */
#include <stdio.h>

int main(void) {
    printf("Hello from Amiga!\n");
    printf("Cross-compiled on Linux.\n");
    return 0;
}
```

Compile:

```bash
m68k-amigaos-gcc -m68000 -O2 -noixemul -o hello hello.c
```

Check the binary:

```bash
# Verify it's an Amiga executable
file hello
# Output: hello: AmigaOS loadseg()ble executable/binary

# Check size
ls -la hello
# Should be ~5-10KB for this simple program
```

### Compiler Flags Explained

Understanding the common flags:

| Flag | Purpose |
|------|---------|
| `-m68000` | Target base 68000 CPU (compatible with all Amigas) |
| `-m68020` | Target 68020+ (A1200, A3000, A4000) |
| `-O2` | Optimization level 2 (good balance of speed/size) |
| `-Os` | Optimize for size (useful for memory-constrained targets) |
| `-noixemul` | Use native AmigaOS calls instead of ixemul.library |
| `-fomit-frame-pointer` | Save stack space (important for 4KB stack limit) |
| `-fno-builtin` | Don't use GCC built-in functions |
| `-Wall` | Enable all warnings (always use this!) |

### Memory Considerations

The classic Amiga has severe memory constraints:

```c
/*
 * IMPORTANT: Amiga memory constraints
 *
 * - Default stack: 4KB (can crash with deep recursion)
 * - Chip RAM: 512KB-2MB (for graphics, sound, DMA)
 * - Fast RAM: 0-8MB (for general use)
 *
 * Avoid:
 * - Large stack allocations (> 256 bytes per function)
 * - Excessive recursion
 * - Memory leaks (no virtual memory!)
 */

/* BAD - allocates 4096 bytes on stack */
void bad_function(void) {
    char buffer[4096];  /* Will crash! */
}

/* GOOD - use static or heap allocation */
static char buffer[4096];  /* Static allocation */

void good_function(void) {
    char *buf = malloc(4096);  /* Heap allocation */
    if (buf) {
        /* use buffer */
        free(buf);
    }
}
```

---

## Setting Up Amiberry

Amiberry is a fast, optimized Amiga emulator perfect for development testing.

### Installing Amiberry

#### On Raspberry Pi:

```bash
# Install from package manager (if available)
sudo apt install amiberry

# Or build from source
git clone https://github.com/BlitterStudio/amiberry.git
cd amiberry
make -j$(nproc) PLATFORM=rpi4-sdl2-dispmanx
```

#### On x86_64 Linux:

```bash
git clone https://github.com/BlitterStudio/amiberry.git
cd amiberry
make -j$(nproc) PLATFORM=x86-64-sdl2
```

### Obtaining Kickstart ROMs

Amiberry requires Amiga Kickstart ROM files. Legal options:

1. **Amiga Forever** - Commercial package with licensed ROMs
   - https://www.amigaforever.com/

2. **Original Amiga** - Dump ROMs from your own hardware
   - Use `TransROM` on a real Amiga

3. **AROS Kickstart** - Open-source alternative (limited compatibility)
   - https://aros.sourceforge.io/

Place ROM files in `~/.config/amiberry/kickstarts/` or specify in config.

### Configuration for Development

Create a development-optimized configuration:

**~/.config/amiberry/conf/development.uae**

```ini
; Amiga 500 development configuration
; Optimized for cross-compilation testing

; CPU - 68000 for maximum compatibility
cpu_type=68000
cpu_speed=max
cpu_compatible=false

; Memory - Maximum for A500
chipmem_size=4        ; 2MB Chip RAM (4 = 2MB in Amiberry units)
fastmem_size=8        ; 8MB Fast RAM
bogomem_size=0        ; No Slow RAM

; Graphics
gfx_width=720
gfx_height=568
gfx_vsync=false
gfx_framerate=1

; Kickstart
kickstart_rom_file=/path/to/kick13.rom  ; or kick31.rom

; Hard drive - mount your project folder
filesystem=rw,DH0:Work:/home/youruser/amiga-projects
filesystem=rw,DH1:RAM:

; Sound (disable for faster testing)
sound_output=none
```

### Setting Up Shared Folders

The key to efficient development is seamless file sharing:

```bash
# Create a projects directory
mkdir -p ~/amiga-projects

# This folder will appear as "Work:" on the emulated Amiga
# Any files you compile appear instantly in Amiberry
```

In your Amiberry config:
```ini
filesystem=rw,Work:/home/youruser/amiga-projects
```

Now when you compile:
```bash
cd ~/amiga-projects/myproject
m68k-amigaos-gcc -o myprogram main.c
# The executable immediately appears in Amiberry as Work:myproject/myprogram
```

---

## Development Workflow

### Recommended Project Structure

```
~/amiga-projects/
  myproject/
    src/
      main.c
      module1.c
      module2.c
    include/
      module1.h
      module2.h
    obj/           # Compiled object files
    Makefile
    README.md
```

### Sample Makefile

```makefile
# Amiga Cross-Compilation Makefile

# Toolchain
CC = m68k-amigaos-gcc
AR = m68k-amigaos-ar
STRIP = m68k-amigaos-strip

# Target CPU (68000 for A500/A1000/A2000, 68020 for A1200+)
CPU = 68000

# Compiler flags
CFLAGS = -m$(CPU) -O2 -Wall -fomit-frame-pointer -noixemul
LDFLAGS = -m$(CPU) -noixemul

# For larger programs, consider size optimization
# CFLAGS = -m$(CPU) -Os -Wall -fomit-frame-pointer -noixemul

# Source files
SRCS = src/main.c src/module1.c src/module2.c
OBJS = $(SRCS:src/%.c=obj/%.o)

# Output
TARGET = myprogram

# Default target
all: $(TARGET)

# Create object directory
obj:
	mkdir -p obj

# Compile source files
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

# Link executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Built $(TARGET) for $(CPU)"
	@ls -la $(TARGET)

# Strip symbols for smaller binary (release builds)
release: $(TARGET)
	$(STRIP) $(TARGET)
	@echo "Stripped release build:"
	@ls -la $(TARGET)

# Clean build artifacts
clean:
	rm -rf obj $(TARGET)

# Run in Amiberry (adjust path as needed)
test: $(TARGET)
	# Copy to Amiberry and launch
	@echo "$(TARGET) ready for testing in Amiberry"

.PHONY: all clean release test
```

### Continuous Development Cycle

```bash
# Terminal 1: Watch for changes and rebuild
watch -n 1 make

# Terminal 2: Amiberry running
amiberry --config development.uae

# In Amiberry Shell:
cd Work:myproject
myprogram
```

For faster iteration, use `inotifywait`:

```bash
# Install inotify-tools
sudo apt install inotify-tools

# Auto-rebuild on save
while inotifywait -e modify src/*.c include/*.h; do
    make && echo "Build successful at $(date)"
done
```

---

## Debugging Tips

### Printf Debugging

The simplest approach - add debug output:

```c
#ifdef DEBUG
#define DBG(fmt, ...) printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

void my_function(int value) {
    DBG("Entering my_function with value=%d", value);
    /* ... */
    DBG("Exiting my_function");
}
```

Compile with `-DDEBUG` to enable:
```bash
m68k-amigaos-gcc -DDEBUG -o myprogram main.c
```

### Memory Debugging

Track allocations manually:

```c
#include <stdio.h>
#include <stdlib.h>

static int alloc_count = 0;
static size_t total_bytes = 0;

void *debug_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (ptr) {
        alloc_count++;
        total_bytes += size;
        printf("[MEM] Alloc %zu bytes at %s:%d (total: %d allocs, %zu bytes)\n",
               size, file, line, alloc_count, total_bytes);
    }
    return ptr;
}

void debug_free(void *ptr, const char *file, int line) {
    if (ptr) {
        alloc_count--;
        printf("[MEM] Free at %s:%d (remaining: %d allocs)\n",
               file, line, alloc_count);
        free(ptr);
    }
}

#define malloc(size) debug_malloc(size, __FILE__, __LINE__)
#define free(ptr) debug_free(ptr, __FILE__, __LINE__)
```

### Stack Usage

Monitor stack consumption:

```c
void check_stack(const char *where) {
    volatile char marker;
    static char *stack_top = NULL;

    if (!stack_top) {
        stack_top = (char *)&marker;
    }

    long used = stack_top - (char *)&marker;
    printf("[STACK] %s: using %ld bytes\n", where, used);

    if (used > 3000) {
        printf("[STACK] WARNING: Approaching 4KB limit!\n");
    }
}
```

### Examining Binaries

```bash
# Check symbol table
m68k-amigaos-nm myprogram | head -20

# Disassemble (requires cross-binutils)
m68k-amigaos-objdump -d myprogram | less

# Check sections and sizes
m68k-amigaos-size myprogram
```

---

## Troubleshooting

### Common Issues

#### "command not found: m68k-amigaos-gcc"

```bash
# Check if toolchain is in PATH
echo $PATH | tr ':' '\n' | grep amiga

# Add to PATH if missing
export PATH="/opt/amiga/m68k-amigaos/bin:$PATH"
```

#### "cannot find -lamiga" or missing libraries

```bash
# Check library path
ls -la /opt/amiga/m68k-amigaos/m68k-amigaos/lib/

# Specify library path explicitly
m68k-amigaos-gcc -L/opt/amiga/m68k-amigaos/m68k-amigaos/lib -o prog main.c
```

#### Program crashes immediately on Amiga

Common causes:
1. **Stack overflow** - Reduce local variable sizes
2. **Missing libraries** - Use `-noixemul` for standalone executables
3. **CPU mismatch** - Compiled for 68020 but running on 68000

```bash
# Verify CPU target
m68k-amigaos-gcc -v 2>&1 | grep -i cpu

# Force 68000 target
m68k-amigaos-gcc -m68000 -o prog main.c
```

#### Amiberry can't see my files

```bash
# Check mount configuration
grep filesystem ~/.config/amiberry/conf/development.uae

# Verify path exists
ls -la ~/amiga-projects

# Check permissions
chmod -R 755 ~/amiga-projects
```

### Getting Help

- **AmigaPorts GitHub**: https://github.com/AmigaPorts/m68k-amigaos-gcc/issues
- **Amiberry GitHub**: https://github.com/BlitterStudio/amiberry/issues
- **EAB Forums**: https://eab.abime.net/
- **Amiga.org**: https://amiga.org/

---

## Next Steps

Now that your environment is set up:

1. Read [Using AmiDB as a Library](02-AmiDB-Library-Usage.md) to learn the API
2. Read [AmiDB Shell Documentation](03-AmiDB-Shell-Guide.md) for the SQL interface
3. Study the AmiDB source code as an example of cross-platform Amiga development

Happy coding!
