CC=gcc
CFLAGS=-lpthread -lm
OUTPUT=p2p

build: main.c
	$(CC) $(CFLAGS) -o $(OUTPUT) $<

debug: main.c
	$(CC) $(CFLAGS) -o debug $< -g

test_serve: main.c
	$(CC) $(CFLAGS) -o debug $< 
	./$(OUTPUT) -serve -name hello.txt -path ./hello.txt


clean:
	rm -rf $(OUTPUT) debug

