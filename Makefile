all: test

clean:
	rm -f test
	rm -f test.o

test.o: test.cpp
	g++ -o test.o test.cpp -std=c++11 -g -c -Wall

test: test.o
	g++ -o test test.o -std=c++11 -g -Wall

# test: test.cpp
# 	g++ -o test test.cpp -std=c++11 -g -Wall
