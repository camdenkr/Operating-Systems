all: fs.o

main: fs.o disk.o
	gcc -o main -Wall -g $^ main.c

fs: fs.o disk.o
	gcc -o $@ -Wall -g $^

%.o: %.c
	gcc -o $@ -Wall -g -c $<

clean:
	rm -f fs *.o *~