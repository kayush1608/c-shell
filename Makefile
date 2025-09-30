# Makefile for the Custom C Shell Project

# Compiler and Flags
# Using flags specified in the project description for POSIX compliance
CC = gcc
CFLAGS = -std=c99 \
         -D_POSIX_C_SOURCE=200809L \
         -Iinclude \
         -Wall -Wextra -g

# Directories
SRCDIR = src
OBJDIR = obj
# The BINDIR has been removed.

# Find all .c files in the source directory
SRCS := $(wildcard $(SRCDIR)/*.c)

# Generate corresponding .o object file names in the obj directory
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

# The final executable name, will be created in the project root.
TARGET = shell.out

# Default target: build the executable
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	@echo "LD  $@"
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile a .c source file into a .o object file
# This creates the object directory if it doesn't exist
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@echo "CC  $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Target to clean up all compiled files and test artifacts
clean:
	@echo "Cleaning all files except source files..."
	@rm -f $(TARGET)
	@find . -maxdepth 1 -type f ! -name "Makefile" -delete
	@rm -rf $(OBJDIR)
	@rm -rf __pycache__
	@rm -rf .pytest_cache
	@rm -rf .shell_test
	@echo "Clean complete. Kept: src/, include/, Makefile"

# Phony targets are not actual files
.PHONY: all clean

