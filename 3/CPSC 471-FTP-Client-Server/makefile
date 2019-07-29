all:client.o server.o
	g++  cli/client.o -o cli/client
	g++  ser/server.o -o ser/server
client.o:
	g++ -c cli/client.cpp -o cli/client.o
server.o:
	g++ -c ser/server.cpp -o ser/server.o
clean:
	rm -r cli/client.o
	rm -r ser/server.o
	rm -r cli/client
	rm -r ser/server

