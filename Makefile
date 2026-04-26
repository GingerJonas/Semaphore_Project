CC=gcc
CFLAGS=-std=gnuc99 -Wall -Wextra -Werror -pedantic -pthread
LDFLAGS=-pthread -lrt
SRC=proj2.c
PROGRAM=proj2

$(PROGRAM): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(PROGRAM) $(LDFLAGS)

run: $(PROGRAM)
	./$(PROGRAM)

clean: $(PROGRAM)
	rm -f $(PROGRAM) proj2.out