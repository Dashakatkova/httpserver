all: compile

compile: clean
	gcc -o httpserver httpserver.c

clean:
	rm httpserver