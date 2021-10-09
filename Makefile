TARGET = wfall
LIBS = -lm -ldl -pthread -lSDL2
CXX = g++
CXXFLAGS = -Wall -O3 -std=c++20 -Iinclude $(shell sdl2-config --cflags)

.PHONY: default all clean run debug

default: $(TARGET)
all: default

BINDIR = bin
SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(SOURCES:src/%.cpp=$(BINDIR)/%.o)
HEADERS = $(wildcard src/*.h)

$(BINDIR)/glad.o: src/glad.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR)/%.o: src/%.cpp $(HEADERS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(CXXFLAGS) $(LIBS) -o $@

clean:
	-rm -f $(BINDIR)/*.o
	-rm -f $(TARGET)

run: $(TARGET)
	-./$(TARGET)

debug: CXXFLAGS += -ggdb -O0
debug: $(TARGET)
	gdb ./$(TARGET)
