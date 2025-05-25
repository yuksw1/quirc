# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -Ilib/quirc
LDFLAGS = # -lm if needed, but typically not for quirc itself

# Target executable
TARGET = qr_decoder.exe

# Source files
# Main application source
SRC_MAIN = main.c

# Quirc library sources (assuming these are the .c files in lib/quirc)
# Adjust if the list of .c files provided by the worker in step 2 was different
SRC_QUIRC = lib/quirc/quirc.c lib/quirc/decode.c lib/quirc/identify.c lib/quirc/version_db.c

# Object files
OBJ_MAIN = $(SRC_MAIN:.c=.o)
OBJ_QUIRC = $(SRC_QUIRC:.c=.o)
OBJS = $(OBJ_MAIN) $(OBJ_QUIRC)

# Default target
all: $(TARGET)

# Rule to link the target executable
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Rule to compile .c files to .o files
# This is a general rule that applies to main.c and quirc .c files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
