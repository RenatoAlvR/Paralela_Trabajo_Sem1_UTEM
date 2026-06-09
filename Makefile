# Compiler settings
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fopenmp

# Libraries to link
LDFLAGS = -lcurl -lssh2

# Target executable name
TARGET = farmacia_parallel

# Source files (add more .cpp files here as you create them)
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) resultados.txt log.txt

.PHONY: all clean