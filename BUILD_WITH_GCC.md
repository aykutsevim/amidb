# Building AmiDB with Bebbo's m68k-amigaos-gcc

This guide shows how to build AmiDB using the modern GCC cross-compiler on Linux.

## Prerequisites

### 1. Install Bebbo's Amiga GCC Toolchain

**Quick install (recommended):**
```bash
cd ~
git clone https://github.com/AmigaPorts/m68k-amigaos-gcc
cd m68k-amigaos-gcc
make all -j$(nproc)
```

This installs to `/opt/amiga` by default.

**Add to PATH:**
```bash
echo 'export PATH="/opt/amiga/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**Verify:**
```bash
m68k-amigaos-gcc --version
```

### 2. Build AmiDB

**On Linux (in the AmiDB directory):**
```bash
cd /home/pi/pimiga/disks/Work/Code/AmiDB

# Build everything
make

# Or build with verbose output
make V=1

# Clean and rebuild
make clean
make
```

### 3. Run on Amiga

Since you're using Amiberry with a shared folder:

**On Amiga (in Shell):**
```
cd Work:Code/AmiDB
./amidb_tests
```

## What Gets Built

- `amidb_tests` - Test executable for Phase 1
- `obj/` - Object files (auto-created)

## Compiler Flags Used

- `-m68000` - Target 68000 CPU (compatible with all Amigas)
- `-O2` - Optimize for speed/size
- `-noixemul` - Don't use ixemul.library (pure AmigaOS)
- `-fomit-frame-pointer` - Save registers on 68000
- `-fno-builtin` - Don't assume C library functions

## Makefile Targets

```bash
make              # Build everything
make clean        # Remove build files
make help         # Show help
make amidb_tests  # Build test executable
```

## Troubleshooting

### "m68k-amigaos-gcc: command not found"
- Make sure toolchain is installed: `which m68k-amigaos-gcc`
- Check PATH: `echo $PATH | grep amiga`
- Re-source bashrc: `source ~/.bashrc`

### Build errors about missing headers
- The toolchain includes AmigaOS NDK headers
- Located in: `/opt/amiga/m68k-amigaos/ndk-include/`
- GCC finds them automatically

### Executable doesn't run on Amiga
- Make sure you used `-m68000` flag (not 68020+)
- Use `-noixemul` flag
- Check file is executable: `protect amidb_tests +s` on Amiga

## Advantages of GCC over VBCC

1. ✅ Better optimizations (modern compiler)
2. ✅ Actively maintained
3. ✅ Better debugging support
4. ✅ Faster compilation
5. ✅ Includes proper NDK headers
6. ✅ Cross-platform development

## File Locations

**On Linux:**
- Source: `/home/pi/pimiga/disks/Work/Code/AmiDB/src/`
- Build: `/home/pi/pimiga/disks/Work/Code/AmiDB/obj/`
- Executable: `/home/pi/pimiga/disks/Work/Code/AmiDB/amidb_tests`

**On Amiga (via shared folder):**
- Project: `Work:Code/AmiDB/`
- Executable: `Work:Code/AmiDB/amidb_tests`

## Next Steps

After Phase 1 tests pass:
1. Continue with Phase 2: Storage Engine
2. Extend Makefile for additional modules
3. Build final `amidb` library and CLI tool

## Support

- m68k-amigaos-gcc: https://github.com/AmigaPorts/m68k-amigaos-gcc
- AmigaOS NDK: Included with toolchain
- AmiDB Spec: See `amidb_specification.md`
