#include "analyzer.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ptx-analyzer <input.ptx> [--dot <output.dot>] [--dominators] [--loops] [--liveness] [--prune-unreachable]\n";
        return 2;
    }
    std::string inputPath = argv[1], dotPath;
    bool showDom = false, showLoops = false, showLiveness = false, prune = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dominators") showDom = true;
        else if (arg == "--loops") showLoops = true;
        else if (arg == "--liveness") showLiveness = true;
        else if (arg == "--prune-unreachable") prune = true;
        else if (arg == "--dot" && i + 1 < argc) dotPath = argv[++i];
        else { std::cerr << "Unknown or incomplete option: " << arg << '\n'; return 2; }
    }
    std::ifstream file(inputPath);
    if (!file) { std::cerr << "Cannot open input file: " << inputPath << '\n'; return 2; }
    std::ostringstream source; source << file.rdbuf();
    try {
        auto program = ptx::analyze(source.str());
        if (prune) {
            const int beforeBlocks = static_cast<int>(program.blocks.size());
            const int beforeInstructions = ptx::activeInstructionCount(program);
            program = ptx::removeUnreachable(program);
            std::cerr << "Unreachable-block removal: " << beforeBlocks << " -> " << program.blocks.size()
                      << " blocks; " << beforeInstructions << " -> " << ptx::activeInstructionCount(program) << " instructions.\n";
        }
        if (!dotPath.empty()) { std::ofstream out(dotPath); if (!out) { std::cerr << "Cannot write DOT file: " << dotPath << '\n'; return 2; } out << ptx::toDot(program); }
        else std::cout << ptx::toDot(program);
        if (showDom) std::cerr << "Dominators:\n" << ptx::dominatorText(program);
        if (showLoops) std::cerr << "Natural loops:\n" << ptx::loopText(program);
        if (showLiveness) std::cerr << "Liveness and register pressure:\n" << ptx::livenessText(program);
    } catch (const std::exception& e) { std::cerr << "PTX analysis error: " << e.what() << '\n'; return 1; }
    return 0;
}
