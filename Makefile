CC = gcc
CFLAGS = -g -Wall -Wvla -fsanitize=address
LDFLAGS =
OBJFILES = shell.o utils.o job_control.o
TARGET = shell

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) $(OBJFILES)