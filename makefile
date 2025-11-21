all: wish.o cmd.o
	gcc cmd.o wish.o -o wish

wish.o: wish.c
	gcc -c wish.c

cmd.o: cmd.c
	gcc -c cmd.c

clean:
	rm *.o wish
