OBJS := main.o chip8.o
TARGET := chip8_emu

CFLAGS := `pkg-config --cflags gtk+-3.0` -Ofast -Wall -Wextra -Wpedantic
LDFLAGS := `pkg-config --libs gtk+-3.0`

all: $(TARGET)

$(TARGET): $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
