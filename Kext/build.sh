#!/usr/bin/env bash

set -e

PROJECT_NAME="Pandora"
BUILD_DIR="build"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"

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
    cmake .. -DCMAKE_BUILD_TYPE=Debug

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
