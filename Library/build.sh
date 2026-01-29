#!/usr/bin/env bash

set -e

PROJECT_NAME="PandoraLibrary"
TARGET_NAME="pdtest"
BUILD_DIR="build"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [COMMAND] [-- ARGS...]"
    echo ""
    echo "Commands:"
    echo "  build     - Build the user-space library test executable (default)"
    echo "  clean     - Clean build artifacts"
    echo "  setup     - Initial CMake setup"
    echo "  rebuild   - Clean and build"
    echo "  run       - Build (if needed) and run ${TARGET_NAME}"
    echo "  help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 build"
    echo "  $0 run -- --help"
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
    log_info "Setting up CMake build system for $PROJECT_NAME..."

    cd "$SOURCE_DIR"

    if [ -d "$BUILD_DIR" ]; then
        log_warning "Build directory already exists"
    else
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Debug

    # Copy compile_commands.json to source root for LSP.
    if [ -f "compile_commands.json" ]; then
        cp compile_commands.json ../
        log_success "Generated compile_commands.json for LSP support"
    fi

    cd "$SOURCE_DIR"
    log_success "CMake setup complete"
}

build_project() {
    log_info "Building $TARGET_NAME..."

    cd "$SOURCE_DIR"

    if [ ! -d "$BUILD_DIR" ]; then
        log_warning "Build directory not found, running setup first..."
        setup_cmake
    fi

    cd "$BUILD_DIR"
    cmake --build . --parallel "$(sysctl -n hw.logicalcpu)"

    if [ -f "compile_commands.json" ]; then
        cp compile_commands.json ../
    fi

    cd "$SOURCE_DIR"
    log_success "Build complete: $SOURCE_DIR/$BUILD_DIR/$TARGET_NAME"
}

clean_project() {
    log_info "Cleaning build artifacts..."

    cd "$SOURCE_DIR"

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "Cleaned build directory"
    fi

    # If an in-source build was done previously, remove common CMake artifacts.
    if [ -f "CMakeCache.txt" ] || [ -d "CMakeFiles" ] || [ -f "Makefile" ] || [ -f "cmake_install.cmake" ]; then
        rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake
        log_success "Removed in-source CMake artifacts"
    fi

    if [ -f "$TARGET_NAME" ]; then
        rm -f "$TARGET_NAME"
        log_success "Removed in-source $TARGET_NAME"
    fi

    if [ -f "compile_commands.json" ]; then
        rm -f "compile_commands.json"
        log_success "Removed compile_commands.json"
    fi
}

run_target() {
    # Split args after '--' so we can pass through flags cleanly.
    shift || true
    if [ "${1:-}" = "--" ]; then
        shift
    fi

    cd "$SOURCE_DIR"

    if [ ! -x "$BUILD_DIR/$TARGET_NAME" ]; then
        build_project
    fi

    log_info "Running $TARGET_NAME $*"
    "$SOURCE_DIR/$BUILD_DIR/$TARGET_NAME" "$@"
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
    "run")
        run_target "$@"
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
