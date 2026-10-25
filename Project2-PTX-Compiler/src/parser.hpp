#pragma once

#include "ast.hpp"
#include "lexer.hpp"

namespace tinyptx {
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parseProgram();
private:
    std::vector<Token> tokens_;
    size_t at_ = 0;
    const Token& current() const;
    bool check(const std::string& text) const;
    bool match(const std::string& text);
    Token consumeIdentifier(const char* message);
    Token consume(const std::string& text, const char* message);
    [[noreturn]] void error(const Token& token, const std::string& message) const;
    Type parseType();
    Kernel parseKernel();
    std::unique_ptr<Block> parseBlock();
    StmtPtr parseStatement();
    ExprPtr parseExpression();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseComparison();
    ExprPtr parseAdditive();
    ExprPtr parseTerm();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
    StmtPtr parseForClause(bool allowDeclaration);
};
}
