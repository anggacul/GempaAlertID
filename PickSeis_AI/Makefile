CC = gcc
CONDA_PREFIX ?= /usr/local
CFLAGS = -std=c11 -g -O2 -fopenmp -Iinclude -I$(CONDA_PREFIX)/include  -L$(CONDA_PREFIX)/lib -D_POSIX_C_SOURCE=200809L
LDFLAGS = -fopenmp -L$(CONDA_PREFIX)/lib -Wl,-rpath,$(CONDA_PREFIX)/lib -lslink -lonnxruntime -lsqlite3 -lm

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = pickseisai

all: $(TARGET)

$(TARGET): pickseisai.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

pickseisai.o: pickseisai.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) pickseisai.o $(TARGET)

.PHONY: all clean 
