all:
	g++ -std=c++11 server.cpp -o server
	g++ -std=c++11 client.cpp -o client

clean:
	rm server client

