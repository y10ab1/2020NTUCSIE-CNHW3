CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`
PTHREAD = -pthread

SENDER = sender.cpp
AGENT = agent.cpp
RECEIVER = receiver.cpp
SEN = sender
AGE = agent
REC = receiver

all: sender agent receiver
  
sender: $(SENDER)
	$(CC) $(SENDER) -o $(SEN)  $(OPENCV) $(PTHREAD) 
agent: $(AGENT)
	$(CC) $(AGENT) -o $(AGE)  $(OPENCV) $(PTHREAD)
receiver: $(RECEIVER)
	$(CC) $(RECEIVER) -o $(REC)  $(OPENCV) $(PTHREAD)

.PHONY: clean

clean:
	rm $(SEN) $(AGE) $(REC)
