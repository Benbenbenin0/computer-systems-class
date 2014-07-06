#
# Makefile for Proxy Lab 
#
# You may modify is file any way you like (except for the handin
# rule). Autolab will execute the command "make" on your specific 
# Makefile to build your proxy from sources.
#
CC = gcc
CFLAGS = -O2 -g -Wall -Werror
LDFLAGS = -lpthread

all: test proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h cache.h
	$(CC) $(CFLAGS) -c proxy.c

test.o: test.c cache.h
	$(CC) $(CFLAGS) -c test.c

cache.o: cache.c cache.h vector.h web_data.h
	$(CC) $(CFLAGS) -c cache.c

vector.o: vector.c vector.h
	$(CC) $(CFLAGS) -c vector.c

web_data.o: web_data.c web_data.h
	$(CC) $(CFLAGS) -c web_data.c

proxy: proxy.o csapp.o cache.o vector.o web_data.o

test: test.o csapp.o cache.o vector.o web_data.o

# Creates a tarball in ../proxylab-handin.tar that you should then
# hand in to Autolab. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz

