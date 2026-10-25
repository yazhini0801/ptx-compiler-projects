#include "codegen.hpp"
#include "diagnostic.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ptxc <input.k> -o <output.ptx> [--dump-ast]\n";
        return 2;
    }
    std::string inputPath = argv[1], outputPath;
    bool dump = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump-ast") dump = true;
        else if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
        else { std::cerr << "Unknown or incomplete option: " << arg << '\n'; return 2; }
    }
    if (outputPath.empty()) { std::cerr << "Missing required -o <output.ptx> option.\n"; return 2; }
    std::ifstream input(inputPath);
    if (!input) { std::cerr << "Cannot open input file: " << inputPath << '\n'; return 2; }
    std::ostringstream source; source << input.rdbuf();
    try {
        tinyptx::Program program = tinyptx::Parser(tinyptx::lex(source.str())).parseProgram();
        tinyptx::checkSemantics(program);
        if (dump) std::cout << tinyptx::dumpAst(program) << '\n';
        const std::string ptx = tinyptx::generatePTX(program);
        std::ofstream output(outputPath);
        if (!output) { std::cerr << "Cannot write output file: " << outputPath << '\n'; return 2; }
        output << ptx;
        std::cerr << "Wrote PTX to " << outputPath << '\n';
    } catch (const tinyptx::CompileError& e) {
        std::cerr << inputPath << ':' << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "ptxc: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
