#!/bin/bash
# Quick installation script for Bebbo's m68k-amigaos-gcc

set -e

echo "========================================"
echo "Installing Bebbo's Amiga GCC Toolchain"
echo "========================================"
echo ""

# Check if already installed
if command -v m68k-amigaos-gcc &> /dev/null; then
    echo "✓ m68k-amigaos-gcc is already installed!"
    m68k-amigaos-gcc --version
    echo ""
    echo "If you want to reinstall, remove /opt/amiga first:"
    echo "  sudo rm -rf /opt/amiga"
    echo "  sudo rm -rf ~/amiga-gcc"
    exit 0
fi

echo "Step 1: Installing dependencies..."
sudo apt-get update
sudo apt-get install -y git make wget curl lhasa python3 \
    gcc g++ libgmp-dev libmpfr-dev libmpc-dev flex \
    bison gettext texinfo ncurses-dev autoconf rsync

echo ""
echo "Step 2: Cloning amiga-gcc repository..."
cd ~
if [ -d "m68k-amigaos-gcc" ]; then
    echo "Directory ~/m68k-amigaos-gcc already exists. Updating..."
    cd m68k-amigaos-gcc
    git pull
else
    git clone https://github.com/AmigaPorts/m68k-amigaos-gcc
    cd m68k-amigaos-gcc
fi

echo ""
echo "Step 3: Building toolchain (this takes 30-60 minutes)..."
echo "You can grab a coffee ☕"
echo ""
echo "Running: make all -j$(nproc)"
make all -j$(nproc)

echo ""
echo "Step 4: Adding to PATH..."
if ! grep -q "/opt/amiga/bin" ~/.bashrc; then
    echo 'export PATH="/opt/amiga/bin:$PATH"' >> ~/.bashrc
    echo "✓ Added to ~/.bashrc"
    source ~/.bashrc
else
    echo "✓ Already in ~/.bashrc"
fi

# Verify installation
echo ""
echo "Step 5: Verifying installation..."
export PATH="/opt/amiga/bin:$PATH"
if command -v m68k-amigaos-gcc &> /dev/null; then
    echo "✓ Success! GCC is working:"
    m68k-amigaos-gcc --version | head -1
else
    echo "⚠ Warning: GCC not found in PATH"
    echo "You may need to run: source ~/.bashrc"
fi

echo ""
echo "========================================"
echo "Installation Complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Open a new terminal (or run: source ~/.bashrc)"
echo "2. Verify: m68k-amigaos-gcc --version"
echo "3. Build AmiDB:"
echo "   cd /home/pi/pimiga/disks/Work/Code/AmiDB"
echo "   make"
echo ""
