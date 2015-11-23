all: compile

compile:
	gcc -o httpserver httpserver.c -pthread

clean:
	rm httpserver