CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
SRC=semaphore.c
PROGRAM=semaphore

$(PROGRAM): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(PROGRAM)

run: $(PROGRAM)
	./$(PROGRAM)

clean: $(PROGRAM)
	rm -f $(PROGRAM)