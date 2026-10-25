#include "ast.hpp"
#include "codegen.hpp"
#include "diagnostic.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {
tinyptx::Program compileSource(const std::string& source) {
    auto program = tinyptx::Parser(tinyptx::lex(source)).parseProgram();
    tinyptx::checkSemantics(program);
    return program;
}
void expectError(const std::string& source, const std::string& expectedText) {
    bool caught = false;
    try { (void)compileSource(source); }
    catch (const std::exception& error) { caught = std::string(error.what()).find(expectedText) != std::string::npos; }
    assert(caught && "program should fail with the expected diagnostic");
}
}

int main() {
    const auto tokens = tinyptx::lex("// ignored\nkernel k(int* p) { int x = 2 + 3 * 4; }");
    assert(tokens.front().text == "kernel");
    assert(tokens.front().line == 2);
    assert(tokens.back().kind == tinyptx::TokenKind::End);

    const auto precedence = compileSource("kernel precedence(int* out, int a, int b, int c) { int x = a + b * c; out[0] = x; }");
    const std::string ast = tinyptx::dumpAst(precedence);
    assert(ast.find("Binary +") != std::string::npos);
    assert(ast.find("Binary *") > ast.find("Binary +"));
    const std::string arithmeticPTX = tinyptx::generatePTX(precedence);
    assert(arithmeticPTX.find(".version 7.8") != std::string::npos);
    assert(arithmeticPTX.find(".target sm_75") != std::string::npos);
    assert(arithmeticPTX.find("ld.param.u64") != std::string::npos);
    assert(arithmeticPTX.find("cvta.to.global.u64") != std::string::npos);
    assert(arithmeticPTX.find("mul.lo.s32") != std::string::npos);
    assert(arithmeticPTX.find("add.s32") != std::string::npos);
    assert(arithmeticPTX.find("st.global.s32") != std::string::npos);

    const auto gidProgram = compileSource("kernel ids(int* out) { out[gid()] = tid(); }");
    const std::string gidPTX = tinyptx::generatePTX(gidProgram);
    assert(gidPTX.find("%ctaid.x") != std::string::npos);
    assert(gidPTX.find("%ntid.x") != std::string::npos);
    assert(gidPTX.find("%tid.x") != std::string::npos);
    assert(gidPTX.find("mad.lo.s32") != std::string::npos);

    const auto floatProgram = compileSource("kernel floats(float* out) { float x = 2.5; out[0] = -x; }");
    const std::string floatPTX = tinyptx::generatePTX(floatProgram);
    assert(floatPTX.find("0f40200000") != std::string::npos);
    assert(floatPTX.find("neg.f32") != std::string::npos);
    assert(floatPTX.find("st.global.f32") != std::string::npos);

    const auto control = compileSource("kernel control(int* out, int n) { int i = 0; while (i < n) { if (i == 2) { out[i] = i; } else { out[i] = 0; } i = i + 1; } }");
    const std::string controlPTX = tinyptx::generatePTX(control);
    assert(controlPTX.find("setp.lt.s32") != std::string::npos);
    assert(controlPTX.find("setp.eq.s32") != std::string::npos);
    assert(controlPTX.find("@!%p") != std::string::npos);
    assert(controlPTX.find("bra L_cond") != std::string::npos);
    assert(controlPTX.find("L_else") != std::string::npos);

    const auto forLogical = compileSource("kernel for_logic(int* out, int n) { int sum = 0; for (int i = 0; i < n; i = i + 1) { if ((i < n && i >= 0) || n == 0) { sum = sum + i; } } out[0] = sum; }");
    const std::string forLogicalPTX = tinyptx::generatePTX(forLogical);
    assert(forLogicalPTX.find("L_logic_short") != std::string::npos);
    assert(forLogicalPTX.find("mov.pred") != std::string::npos);
    assert(forLogicalPTX.find("L_cond") != std::string::npos);
    assert(tinyptx::dumpAst(forLogical).find("For") != std::string::npos);

    const auto twoKernels = compileSource("kernel first(int* p) { p[0] = 1; } kernel second(float* p) { p[0] = 1.0; }");
    assert(twoKernels.kernels.size() == 2);
    assert(tinyptx::generatePTX(twoKernels).find(".visible .entry second") != std::string::npos);

    expectError("kernel k(int* p) { p[0] = missing; }", "undeclared variable 'missing'");
    expectError("kernel k(int* p) { int x = 0; int x = 1; }", "duplicate declaration of 'x'");
    expectError("kernel k(int x) { x[0] = 1; }", "cannot index non-pointer");
    expectError("kernel k(int* p) { int x = 1; float y = 2.0; p[0] = x + y; }", "mixed or incompatible types");
    expectError("kernel k(int* p) { int x = 1; if (x) p[0] = x; }", "condition must be a comparison");
    expectError("kernel k(int* p) { p[0] = 1.0; }", "cannot assign float to int");
    expectError("kernel k(int* p) { (p[0]) = 1; }", "expected declaration");
    expectError("kernel k(int* p) { p[0] = 1 @; }", "invalid character '@'");
    expectError("kernel k(int* p) {\n p[0] = 1\n}", "expected ';' after assignment");
    expectError("kernel k(int* p) {\n p[0] = 1;\n", "expected '}' to close block");
    expectError("kernel k(int* p) {\n p[0] = gid(1);\n}", "built-in takes no arguments");
    expectError("kernel k(int* p) { if (1 < 2 && 3) p[0] = 1; }", "logical operator '&&' requires comparison operands");

    std::cout << "All Project 2 compiler phase tests passed.\n";
}
