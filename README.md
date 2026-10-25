# PTX Compiler Internship Projects

This repository contains two related C++ projects focused on NVIDIA PTX and compiler fundamentals.

## Projects

- [Project 1: PTX Analyzer](Project1-PTX-Analyzer/README.md) parses PTX, builds control-flow graphs, and performs dominator, natural-loop, liveness, register-pressure, and unreachable-block analyses.
- [Project 2: Tiny GPU Language to PTX Compiler](Project2-PTX-Compiler/README.md) compiles a small C-like GPU language to PTX, with tests for parsing, semantic errors, control flow, and PTX generation.

Each project has its own build instructions, tests, examples, and limitations. Project 2 includes a hand trace of generated PTX and Colab assembler screenshots. Its generated `vadd` PTX was assembled with `ptxas` and executed on a Colab Tesla T4; all 1,024 results matched expected values.

## Build

Build and test either project separately from its folder. On Linux or WSL, use `make` and `make test`. On Windows, follow the Visual Studio Developer PowerShell instructions in each project README.
