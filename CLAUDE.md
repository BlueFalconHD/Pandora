# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **Pandora**, a macOS kernel extension (KEXT) written in C++ that provides kernel-level memory access capabilities. The project creates a `.kext` bundle that interfaces with macOS kernel APIs through the IOKit framework.

**⚠️ Security Notice**: This is a kernel extension that provides low-level system access. Any modifications should be approached with extreme caution and only for legitimate defensive security purposes.

## Architecture

- **Main IOService**: `Pandora` class (src/Pandora.h/cpp) - Core kernel extension service
- **User Client**: `PandoraUserClient` class (src/PandoraUserClient.h/cpp) - Handles userspace communication
- **Utilities**: Various helper modules for kernel operations and logging
- **Assembly**: Low-level routines in `src/routines.S`

The KEXT follows standard IOKit patterns with IOService inheritance and IOUserClient for userspace communication.

## Build System

The project supports both CMake and Make build systems:

### Primary Build Commands

**Using build.sh (recommended)**:
- `./build.sh` - Build the kernel extension
- `./build.sh clean` - Clean build artifacts
- `./build.sh rebuild` - Clean and rebuild
- `./build.sh install` - Install the KEXT (requires sudo)
- `./build.sh setup` - Initial CMake setup

**Using Make directly**:
- `make` - Build the KEXT
- `make install` - Install to `/Library/Extensions` (requires sudo)
- `make clean` - Clean build artifacts

**Using CMake directly**:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel $(sysctl -n hw.logicalcpu)
```

### Development Setup

- The build process generates `compile_commands.json` for LSP support
- Target architecture: `arm64e` (Apple Silicon)
- C++ standard: C++17
- Kernel-specific flags: `-mkernel`, `-nostdlib`, `-fno-exceptions`, `-fno-rtti`

## Key Files

- `CMakeLists.txt` - Primary build configuration
- `Makefile` - Alternative build system
- `misc/Info.plist` - KEXT bundle metadata and IOKit personality
- `build.sh` - Unified build script with colored output
- `scripts/bump` - Version bumping utility
- `scripts/inst` - Installation helper
- `scripts/clean` - Cleanup helper

## Version Management

Use `scripts/bump` to increment the version in `misc/Info.plist`. The versioning scheme uses `MAJOR.MINOR.MINISCULE` with automatic rollover.

## Installation Notes

- KEXT installation requires sudo privileges
- The extension is installed to `/Library/Extensions/`
- Code signing is handled automatically during build
- System Integrity Protection (SIP) may need to be considered for loading