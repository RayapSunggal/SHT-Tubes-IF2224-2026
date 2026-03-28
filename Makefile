CC = gcc
CFLAGS ?= -Wall -Wextra -std=c11
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $2,$d))
SRC := $(call rwildcard,src/,%.c)
TARGET := app

.PHONY: build run clean

build:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: build
	./$(TARGET)

clean:
	@rm -f $(TARGET)
