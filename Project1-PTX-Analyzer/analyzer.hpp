#pragma once

#include <set>
#include <string>
#include <vector>

namespace ptx {
struct Instruction {
    std::string opcode;
    std::string operands;
    std::string source;
    std::vector<std::string> uses;
    std::vector<std::string> definitions;
    std::string predicate;
    int functionId = 0;
    int line = 0;
};

struct FunctionInfo {
    int id = 0;
    std::string name;
    int entryInstruction = 0;
};

struct BasicBlock {
    int id = 0;
    std::vector<int> instructions;
    std::vector<std::string> labels;
    std::vector<int> successors;
    std::vector<int> predecessors;
    int functionId = 0;
    bool isEntry = false;
};

struct NaturalLoop {
    int header = 0;
    int latch = 0;
    std::vector<int> blocks;
};

struct Program {
    std::vector<Instruction> instructions;
    std::vector<FunctionInfo> functions;
    std::vector<int> instructionFunctions;
    std::vector<BasicBlock> blocks;
    std::vector<std::vector<int>> dominators;
    std::vector<NaturalLoop> loops;
    std::vector<std::set<std::string>> liveIn;
    std::vector<std::set<std::string>> liveOut;
    std::vector<bool> reachableBlocks;
    int maxPressure = 0;
    int maxPressureBlock = -1;
};

Program analyze(const std::string& source);
Program removeUnreachable(const Program& program);
int activeInstructionCount(const Program& program);
std::string toDot(const Program& program);
std::string dominatorText(const Program& program);
std::string loopText(const Program& program);
std::string livenessText(const Program& program);
}
