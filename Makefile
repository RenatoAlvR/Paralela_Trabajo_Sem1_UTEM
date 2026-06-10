CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fopenmp
LDFLAGS = -lcurl -lssh2
TARGET = farmacia_parallel
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) resultados.txt log.txt

.PHONY: all clean