CC = gcc
CFLAGS = -Og -pipe -Wall -g

INCLUDE = -Iinclude/ -I/usr/include/
LIBS =

CL = $(wildcard src/*.c)    #cpp list
HL = $(wildcard include/*.h)    #header list
OL = $(patsubst src/%.c, obj/%.o, $(CL) )  #object list

all: $(OL); $(CC) $(CFLAGS) $(OL) $(LIBS) $(INCLUDE) -o main

obj/%.o: src/%.c; $(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

test:
	$(CC) servidor.c -o servidor
	$(CC) cliente.c -o cliente

run: ; ./main

clean: ; rm main obj/*.o