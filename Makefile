FLAGS := -Wunused-variable -Wreturn-type

filenames := test1.o http_server.o test1
files := $(strip $(foreach f,$(filenames),$(wildcard $(f))))

build: clean http_server.o

test: clean debug http_server.o test1.o
	g++ -g -o test1 test1.o http_server.o

test1.o:
	g++ -g -c -pthread $(FLAGS) -Isrc test/test1.cpp

http_server.o:
	g++ -g -c -pthread $(FLAGS) -Isrc src/http_server.cpp

.PHONY: debug
debug:
	$(eval FLAGS += -DDEBUG)

clean:
ifneq ($(files),)
	rm -f $(files)
endif