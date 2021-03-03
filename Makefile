FLAGS := -pthread -Wunused-variable -Wreturn-type

build: clean http_server.o

test: clean debug http_server.o test1.o
	g++ $(FLAGS) test1.o http_server.o -o test1
	$(MAKE) cleano

test1.o:
	g++ -c $(FLAGS) -Isrc test/test1.cpp

http_server.o:
	g++ -c $(FLAGS) -Isrc src/http_server.cpp

debug:
	$(eval FLAGS += -g -DDEBUG)

clean: cleano
	rm -f test1 http_server

cleano:
	rm -f *.o