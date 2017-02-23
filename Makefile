.PHONY: clean

CFLAGS 	:= -Wall -Werror -g -pg -std=c++11 -O0
LD	:= g++
LDFLAGS	:= ${LDFLAGS} -lrdmacm -libverbs -lpthread

buf_list.o: buf_list.cpp
	${LD} -o $@ $^ -c ${CFLAGS} ${LDFLAGS}

rdma.o: rdma.cpp 
	${LD} -o $@ $^ -c ${CFLAGS} ${LDFLAGS}

test: rdma.o buf_list.o test.cpp
	${LD} -o $@ $^ ${CFLAGS} ${LDFLAGS}

clean:
	rm -f *.o test

