CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -I../components/weld_parser
LDFLAGS = -lm
SRCS    = test_weld_parser.c ../components/weld_parser/weld_parser.c
TARGET  = test_weld_parser

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
