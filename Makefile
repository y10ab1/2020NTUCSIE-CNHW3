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

SENDER = sender.cpp
RECEIVER = receiver.cpp
SND = sender
REV = receiver
all: openCV agent sender receiver
  
server: $(SERVER)
	$(CC) $(SERVER) -o $(SER)  $(OPENCV) $(PTHREAD) $(c11)
client: $(CLIENT)
	$(CC) $(CLIENT) -o $(CLI)  $(OPENCV) $(PTHREAD) $(c11)
opencv: openCV.cpp
	$(CC) openCV.cpp -o openCV $(OPENCV) $(c11)
agent: agent.c
	$(CC) agent.cpp -o agent $(OPENCV) $(c11)
sender: $(SENDER)
	$(CC) $(SENDER) -o $(SND)  $(OPENCV) $(PTHREAD) $(c11)
receiver: $(RECEIVER)
	$(CC) $(RECEIVER) -o $(REV)  $(OPENCV) $(PTHREAD) $(c11)
testc: $(testc)
	$(CC) $(testc) -o testc  $(OPENCV) $(PTHREAD) $(c11) 
tests: $(tests)
	$(CC) $(tests) -o tests  $(OPENCV) $(PTHREAD) $(c11)

.PHONY: clean

clean:
	rm $(CLI) $(SER) openCV $(REV) $(SND)
