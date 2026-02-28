CC = clang
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lsqlite3

# Attempt to use pkg-config for sqlite3 if available
PKG_CONFIG := $(shell command -v pkg-config 2> /dev/null)

ifdef PKG_CONFIG
    CFLAGS += $(shell pkg-config --cflags sqlite3)
    LDFLAGS += $(shell pkg-config --libs sqlite3)
else
    # Fallback to standard macOS/Homebrew/Linux paths
    CFLAGS += -I/opt/homebrew/include -I/usr/local/include
    LDFLAGS += -L/opt/homebrew/lib -L/usr/local/lib
endif

SRC = src/main.c src/db.c
OBJ = $(SRC:.c=.o)
TARGET = uatu

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean