CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude
SRCS = src/main.cpp src/fs.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = minifs

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp include/fs.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
