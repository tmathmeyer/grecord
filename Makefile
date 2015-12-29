all:
	gcc -o grecord -lX11 -lpthread -pedantic-errors record.c

clean:
	rm grecord
