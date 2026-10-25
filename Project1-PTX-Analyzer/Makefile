CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

.PHONY: all test clean
all: ptx-analyzer

ptx-analyzer: main.cpp analyzer.cpp analyzer.hpp
	$(CXX) $(CXXFLAGS) main.cpp analyzer.cpp -o $@

test: test-analyzer
	./test-analyzer

test-analyzer: tests/test_analyzer.cpp analyzer.cpp analyzer.hpp
	$(CXX) $(CXXFLAGS) -I. tests/test_analyzer.cpp analyzer.cpp -o $@

clean:
	rm -f ptx-analyzer test-analyzer
