CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = game_server

all: $(TARGET)

$(TARGET): server.c gamedata.c
	$(CC) $(CFLAGS) -o $(TARGET) server.c gamedata.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)