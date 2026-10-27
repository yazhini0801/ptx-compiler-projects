CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SOURCES := src/elf_reader.cpp src/print_header.cpp src/print_sections.cpp src/print_symbols.cpp src/print_relocs.cpp src/resolve.cpp src/main.cpp

.PHONY: all test sanitize clean
all: elfinspect

elfinspect: $(SOURCES) src/elf_reader.hpp src/elf_types.hpp src/reports.hpp
	$(CXX) $(CXXFLAGS) -Isrc $(SOURCES) -o $@

test: elfinspect
	python3 tests/run_tests.py

sanitize:
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc $(SOURCES) -o elfinspect-sanitize
	ELFINSPECT=./elfinspect-sanitize python3 tests/run_tests.py

clean:
	rm -f elfinspect elfinspect-sanitize
	rm -rf tests/generated
