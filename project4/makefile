CC=gcc -Werror -Wall -g

all: tls main.c
	$(CC) -lpthread -o main main.c tls.o

tls: tls.c
	$(CC) -lpthread -c -o tls.o tls.c
	
clean:
	rm main tls.o
