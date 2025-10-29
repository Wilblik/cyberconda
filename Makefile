CC = gcc

TARGET = cyberconda

SRCS = main.c
OBJS = $(SRCS:.c=.o)

CCFLAGS = -Wall -Wextra -g `pkg-config --cflags sdl3`
LDFLAGS = `pkg-config --libs sdl3`

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
