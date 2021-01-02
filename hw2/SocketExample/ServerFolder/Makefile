CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread

C11 = -std=c++11

CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server
testc = testc.cpp
tests = tests.cpp

all: server client openCV
  
server: $(SERVER)
	$(CC) $(SERVER) -o $(SER)  $(OPENCV) $(PTHREAD) $(c11)
client: $(CLIENT)
	$(CC) $(CLIENT) -o $(CLI)  $(OPENCV) $(PTHREAD) $(c11)
opencv: openCV.cpp
	$(CC) openCV.cpp -o openCV $(OPENCV) $(c11)
testc: $(testc)
	$(CC) $(testc) -o testc  $(OPENCV) $(PTHREAD) $(c11) 
tests: $(tests)
	$(CC) $(tests) -o tests  $(OPENCV) $(PTHREAD) $(c11)

.PHONY: clean

clean:
	rm $(CLI) $(SER) openCV
