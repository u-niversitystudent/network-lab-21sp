all: client server

client: http-client.c
	@rm -f http-client
	gcc -Wall -g http-client.c -o http-client

server: http-server.c
	gcc -Wall -g http-server.c -lpthread -o http-server

clean:
	@rm -f http-client http-server
