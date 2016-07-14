CC:=gcc
WARN:=-Wall -DMW_STDIO

all:procnanny.server procnanny.client

procnanny.server:procnanny.server.c
	$(CC) $(WARN) $^ -o procnanny.server

procnanny.client:procnanny.client.c
	$(CC) $(WARN) $^ -o procnanny.client

clean:
	rm -rf *.o procnanny.server procnanny.client
