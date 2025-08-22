TESTS = $(wildcard *.c)
CC = clang
CFLAGS = -Wall -Wextra
#-I../build/include
# LDFLAGS =
FRAMEWORKS = -framework IOKit -framework CoreFoundation
BUILD_DIR = ./build

all:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TESTS) -o $(BUILD_DIR)/pdtest $(FRAMEWORKS)
	ldid -Sentitlements.plist $(BUILD_DIR)/pdtest

clean:
	rm $(BUILD_DIR)/pdtest
