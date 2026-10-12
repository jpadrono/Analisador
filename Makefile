CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Wpedantic -O2
TARGET := compilador
SOURCES := src/main.cpp src/lexer.cpp src/lalr.cpp src/compiler.cpp
HEADERS := src/lexer.hpp src/lalr.hpp src/compiler.hpp

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

lexer_tests: tests/lexer_tests.cpp src/lexer.cpp src/lexer.hpp
	$(CXX) $(CXXFLAGS) tests/lexer_tests.cpp src/lexer.cpp -o lexer_tests

compiler_tests: tests/compiler_tests.cpp src/lexer.cpp src/lalr.cpp src/compiler.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) tests/compiler_tests.cpp src/lexer.cpp src/lalr.cpp src/compiler.cpp -o compiler_tests

test: $(TARGET) lexer_tests compiler_tests
	./lexer_tests
	./compiler_tests
	./$(TARGET) examples/fatorial.ssl

clean:
	$(RM) $(TARGET) $(TARGET).exe lexer_tests lexer_tests.exe compiler_tests compiler_tests.exe
