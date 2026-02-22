#!/usr/bin/env bash

set -e

PROJECT_NAME="Pandora"
BUILD_DIR="build"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"

# Building as root will leave root-owned artifacts in the build directory. The
# install step already uses sudo internally where required.
if [ "${EUID:-$(id -u)}" -eq 0 ]; then
    echo "[ERROR] Do not run this script with sudo. Build as your user; only the install step elevates." >&2
    exit 1
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  build     - Build the kernel extension (default)"
    echo "  clean     - Clean build artifacts"
    echo "  install   - Install the kernel extension"
    echo "  setup     - Initial CMake setup"
    echo "  rebuild   - Clean and build"
    echo "  help      - Show this help message"
    echo ""
    echo "The build generates compile_commands.json for LSP support."
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

setup_cmake() {
    log_info "Setting up CMake build system..."

    # Ensure we're in the source directory
    cd "$SOURCE_DIR"

    if [ -d "$BUILD_DIR" ]; then
        log_warning "Build directory already exists"
    else
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    # The kernel headers in the macOS SDK rely on AppleClang builtins (e.g.
    # __builtin_ptrauth_string_discriminator). Using non-Apple clang (such as
    # Homebrew LLVM) can fail with errors like:
    #   "statement expression not allowed at file scope"
    local C_COMPILER CXX_COMPILER
    C_COMPILER="$(xcrun --find clang)"
    CXX_COMPILER="$(xcrun --find clang++)"
    local SDK_PATH
    SDK_PATH="$(xcrun --show-sdk-path)"

    if [ -z "$C_COMPILER" ] || [ -z "$CXX_COMPILER" ] || [ -z "$SDK_PATH" ]; then
        log_error "Failed to locate Xcode toolchain compilers via xcrun."
        log_error "Ensure Xcode (or Command Line Tools) are installed and selected (xcode-select)."
        exit 1
    fi
    if [ ! -d "$SDK_PATH" ]; then
        log_error "macOS SDK path reported by xcrun does not exist: $SDK_PATH"
        exit 1
    fi

    # If the build dir was previously configured with a different compiler,
    # re-configure to avoid sticky CMake cache issues.
    if [ -f "CMakeCache.txt" ]; then
        local CACHED_CXX
        CACHED_CXX="$(grep -E '^CMAKE_CXX_COMPILER:FILEPATH=' CMakeCache.txt | cut -d= -f2- || true)"
        if [ -n "$CACHED_CXX" ] && [ "$CACHED_CXX" != "$CXX_COMPILER" ]; then
            log_warning "Build dir is configured for a different compiler:"
            log_warning "  cached: $CACHED_CXX"
            log_warning "  desired: $CXX_COMPILER"
            log_warning "Removing CMake cache to reconfigure with AppleClang..."
            rm -rf CMakeCache.txt CMakeFiles
        fi
    fi

    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER="$C_COMPILER" \
        -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
        -DCMAKE_OSX_SYSROOT="$SDK_PATH"

    # Copy compile_commands.json to root for LSP
    if [ -f "compile_commands.json" ]; then
        cp compile_commands.json ../
        log_success "Generated compile_commands.json for LSP support"
    fi

    cd "$SOURCE_DIR"
    log_success "CMake setup complete"
}

build_project() {
    log_info "Building $PROJECT_NAME..."

    # Ensure we're in the source directory
    cd "$SOURCE_DIR"

    if [ ! -d "$BUILD_DIR" ]; then
        log_warning "Build directory not found, running setup first..."
        setup_cmake
        cd "$SOURCE_DIR"
    fi

    # Double-check build directory exists after setup
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory still not found after setup"
        exit 1
    fi

    cd "$BUILD_DIR"
    cmake --build . --parallel $(sysctl -n hw.logicalcpu)

    # Update compile_commands.json
    if [ -f "compile_commands.json" ]; then
        cp compile_commands.json ../
    fi

    cd "$SOURCE_DIR"
    log_success "Build complete: $BUILD_DIR/$PROJECT_NAME.kext"
}

clean_project() {
    log_info "Cleaning build artifacts..."

    # Ensure we're in the source directory
    cd "$SOURCE_DIR"

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "Cleaned build directory"
    fi

    if [ -f "compile_commands.json" ]; then
        rm -f "compile_commands.json"
        log_success "Removed compile_commands.json"
    fi

    # Remove any .kext files in root
    if ls *.kext 1> /dev/null 2>&1; then
        rm -rf *.kext
        log_success "Removed .kext files from root"
    fi
}

install_kext() {
    # Ensure we're in the source directory
    cd "$SOURCE_DIR"

    if [ ! -f "$BUILD_DIR/$PROJECT_NAME.kext/Contents/MacOS/$PROJECT_NAME" ]; then
        log_error "Kernel extension not built. Run 'build' first."
        exit 1
    fi

    log_info "Installing $PROJECT_NAME.kext..."
    log_warning "This requires sudo privileges"

    cd "$BUILD_DIR"
    cmake --build . --target install_kext
    cd "$SOURCE_DIR"

    log_success "Installation complete"
    log_info "You may need to restart or reload kernel extensions"
}

# Main script logic
case "${1:-build}" in
    "build")
        build_project
        ;;
    "setup")
        setup_cmake
        ;;
    "clean")
        clean_project
        ;;
    "rebuild")
        clean_project
        build_project
        ;;
    "install")
        install_kext
        ;;
    "help"|"-h"|"--help")
        print_usage
        ;;
    *)
        log_error "Unknown command: $1"
        print_usage
        exit 1
        ;;
esac
