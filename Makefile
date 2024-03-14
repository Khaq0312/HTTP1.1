CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra

TARGET = 21127623

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(TARGET).cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
