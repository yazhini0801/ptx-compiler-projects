CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
SOURCES = src/ast.cpp src/lexer.cpp src/parser.cpp src/sema.cpp src/codegen.cpp

.PHONY: all test clean
all: ptxc

ptxc: $(SOURCES) src/main.cpp
	$(CXX) $(CXXFLAGS) $(SOURCES) src/main.cpp -o $@

test-compiler: $(SOURCES) tests/test_compiler.cpp
	$(CXX) $(CXXFLAGS) -Isrc $(SOURCES) tests/test_compiler.cpp -o $@

test: ptxc test-compiler
	./test-compiler
	python3 tests/run_tests.py ./ptxc

clean:
	rm -f ptxc test-compiler
