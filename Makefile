#Scalable for any amount of files
CC = gcc
CFLAGS = -g -Wall

TARGET = project1
OBJS = project1.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@