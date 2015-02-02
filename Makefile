CC=g++
CFLAGS=-g -c -Wall -Wextra -Werror
CXXFLAGS=-std=c++14
LDFLAGS=
SOURCES=src/test.cpp src/memory_pool.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=test

all: $(SOURCES) $(EXECUTABLE)

clean:
	rm $(EXECUTABLE) $(OBJECTS)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $(CXXFLAGS) $< -o $@

