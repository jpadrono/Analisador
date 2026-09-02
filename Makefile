CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Wpedantic -O2
TARGET := analisador_lexico
SOURCES := src/main.cpp src/lexer.cpp
TEST_TARGET := lexer_tests

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES) src/lexer.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

$(TEST_TARGET): tests/lexer_tests.cpp src/lexer.cpp src/lexer.hpp
	$(CXX) $(CXXFLAGS) tests/lexer_tests.cpp src/lexer.cpp -o $(TEST_TARGET)

test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET)
	./$(TARGET) examples/valido.ssl

clean:
	$(RM) $(TARGET) $(TARGET).exe $(TEST_TARGET) $(TEST_TARGET).exe
