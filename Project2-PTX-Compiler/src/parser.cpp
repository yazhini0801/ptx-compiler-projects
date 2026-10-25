#include "parser.hpp"
#include "diagnostic.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace tinyptx {
Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
const Token& Parser::current() const { return tokens_[at_]; }
bool Parser::check(const std::string& text) const { return current().text == text; }
bool Parser::match(const std::string& text) { if (!check(text)) return false; ++at_; return true; }
Token Parser::consumeIdentifier(const char* message) {
    if (current().kind != TokenKind::Identifier) error(current(), message);
    return tokens_[at_++];
}
Token Parser::consume(const std::string& text, const char* message) {
    if (!check(text)) error(current(), message);
    return tokens_[at_++];
}
[[noreturn]] void Parser::error(const Token& token, const std::string& message) const { throw CompileError(token.line, token.column, message); }

Program Parser::parseProgram() {
    Program program;
    while (current().kind != TokenKind::End) program.kernels.push_back(parseKernel());
    if (program.kernels.empty()) error(current(), "a program must contain at least one kernel");
    return program;
}
Type Parser::parseType() {
    Type type;
    if (match("int")) type.base = Type::Base::Int;
    else if (match("float")) type.base = Type::Base::Float;
    else error(current(), "expected type 'int' or 'float'");
    type.pointer = match("*");
    return type;
}
Kernel Parser::parseKernel() {
    const Token start = consume("kernel", "expected 'kernel'");
    const Token name = consumeIdentifier("expected kernel name");
    consume("(", "expected '(' after kernel name");
    Kernel kernel{start.line, name.text, {}, nullptr};
    if (!check(")")) {
        do {
            const Type type = parseType();
            const Token param = consumeIdentifier("expected parameter name");
            kernel.params.push_back({param.line, type, param.text});
        } while (match(","));
    }
    consume(")", "expected ')' after parameters");
    kernel.body = parseBlock();
    return kernel;
}
std::unique_ptr<Block> Parser::parseBlock() {
    const Token open = consume("{", "expected '{' to start block");
    auto block = std::make_unique<Block>(open.line);
    while (!check("}") && current().kind != TokenKind::End) block->statements.push_back(parseStatement());
    consume("}", "expected '}' to close block");
    return block;
}
StmtPtr Parser::parseStatement() {
    if (check("{")) return parseBlock();
    if (check("int") || check("float")) {
        const Token start = current(); const Type type = parseType();
        const Token name = consumeIdentifier("expected variable name");
        ExprPtr initializer;
        if (match("=")) initializer = parseExpression();
        consume(";", "expected ';' after declaration");
        return std::make_unique<VarDecl>(start.line, type, name.text, std::move(initializer));
    }
    if (match("if")) {
        const Token start = tokens_[at_ - 1];
        consume("(", "expected '(' after 'if'"); ExprPtr condition = parseExpression();
        consume(")", "expected ')' after condition"); StmtPtr thenBranch = parseStatement();
        StmtPtr elseBranch;
        if (match("else")) elseBranch = parseStatement();
        return std::make_unique<IfStmt>(start.line, std::move(condition), std::move(thenBranch), std::move(elseBranch));
    }
    if (match("while")) {
        const Token start = tokens_[at_ - 1];
        consume("(", "expected '(' after 'while'"); ExprPtr condition = parseExpression();
        consume(")", "expected ')' after condition"); StmtPtr body = parseStatement();
        return std::make_unique<WhileStmt>(start.line, std::move(condition), std::move(body));
    }
    if (match("for")) {
        const Token start = tokens_[at_ - 1];
        consume("(", "expected '(' after 'for'");
        StmtPtr initializer;
        if (!check(";")) initializer = parseForClause(true);
        consume(";", "expected ';' after for initializer");
        ExprPtr condition;
        if (!check(";")) condition = parseExpression();
        consume(";", "expected ';' after for condition");
        StmtPtr step;
        if (!check(")")) step = parseForClause(false);
        consume(")", "expected ')' after for clauses");
        StmtPtr body = parseStatement();
        return std::make_unique<ForStmt>(start.line, std::move(initializer), std::move(condition), std::move(step), std::move(body));
    }
    const Token target = consumeIdentifier("expected declaration, assignment, if, while, or block");
    ExprPtr index;
    if (match("[")) { index = parseExpression(); consume("]", "expected ']' after index"); }
    consume("=", "expected '=' in assignment");
    ExprPtr value = parseExpression(); consume(";", "expected ';' after assignment");
    return std::make_unique<Assign>(target.line, target.text, std::move(index), std::move(value));
}
ExprPtr Parser::parseExpression() { return parseLogicalOr(); }
ExprPtr Parser::parseLogicalOr() {
    ExprPtr expr = parseLogicalAnd();
    while (match("||")) { const Token op = tokens_[at_ - 1]; ExprPtr rhs = parseLogicalAnd(); expr = std::make_unique<BinaryExpr>(op.line, op.text, std::move(expr), std::move(rhs)); }
    return expr;
}
ExprPtr Parser::parseLogicalAnd() {
    ExprPtr expr = parseComparison();
    while (match("&&")) { const Token op = tokens_[at_ - 1]; ExprPtr rhs = parseComparison(); expr = std::make_unique<BinaryExpr>(op.line, op.text, std::move(expr), std::move(rhs)); }
    return expr;
}
ExprPtr Parser::parseComparison() {
    ExprPtr left = parseAdditive();
    if (check("<") || check("<=") || check(">") || check(">=") || check("==") || check("!=")) {
        const Token op = tokens_[at_++]; ExprPtr right = parseAdditive();
        return std::make_unique<BinaryExpr>(op.line, op.text, std::move(left), std::move(right));
    }
    return left;
}
ExprPtr Parser::parseAdditive() {
    ExprPtr expr = parseTerm();
    while (check("+") || check("-")) { const Token op = tokens_[at_++]; ExprPtr rhs = parseTerm(); expr = std::make_unique<BinaryExpr>(op.line, op.text, std::move(expr), std::move(rhs)); }
    return expr;
}
ExprPtr Parser::parseTerm() {
    ExprPtr expr = parseUnary();
    while (check("*") || check("/")) { const Token op = tokens_[at_++]; ExprPtr rhs = parseUnary(); expr = std::make_unique<BinaryExpr>(op.line, op.text, std::move(expr), std::move(rhs)); }
    return expr;
}
ExprPtr Parser::parseUnary() {
    if (match("-")) { const Token op = tokens_[at_ - 1]; return std::make_unique<UnaryExpr>(op.line, op.text, parseUnary()); }
    return parsePrimary();
}
ExprPtr Parser::parsePrimary() {
    const Token token = current();
    if (token.kind == TokenKind::Integer) {
        ++at_;
        const long long value = std::strtoll(token.text.c_str(), nullptr, 10);
        if (value > std::numeric_limits<int32_t>::max()) error(token, "integer literal is outside the 32-bit signed range");
        return std::make_unique<IntLiteral>(token.line, static_cast<int32_t>(value));
    }
    if (token.kind == TokenKind::Float) {
        ++at_;
        char* end = nullptr; const float value = std::strtof(token.text.c_str(), &end);
        if (end == token.text.c_str() || *end != '\0' || !std::isfinite(value)) error(token, "invalid or out-of-range float literal");
        return std::make_unique<FloatLiteral>(token.line, value);
    }
    if (token.kind == TokenKind::Identifier) {
        ++at_;
        if (token.text == "gid" || token.text == "tid") {
            consume("(", "expected '(' after built-in name"); consume(")", "built-in takes no arguments");
            return std::make_unique<BuiltinExpr>(token.line, token.text == "gid" ? BuiltinExpr::Kind::Gid : BuiltinExpr::Kind::Tid);
        }
        if (check("(")) error(current(), "only gid() and tid() function calls are supported");
        if (match("[")) { ExprPtr index = parseExpression(); consume("]", "expected ']' after index"); return std::make_unique<IndexExpr>(token.line, token.text, std::move(index)); }
        return std::make_unique<VarRef>(token.line, token.text);
    }
    if (match("(")) { ExprPtr expr = parseExpression(); consume(")", "expected ')' after expression"); return expr; }
    error(token, "expected a literal, variable, built-in, or parenthesized expression");
}
StmtPtr Parser::parseForClause(bool allowDeclaration) {
    if (allowDeclaration && (check("int") || check("float"))) {
        const Token start = current(); const Type type = parseType();
        const Token name = consumeIdentifier("expected variable name in for initializer");
        ExprPtr initializer;
        if (match("=")) initializer = parseExpression();
        return std::make_unique<VarDecl>(start.line, type, name.text, std::move(initializer));
    }
    const Token target = consumeIdentifier("expected assignment in for clause");
    ExprPtr index;
    if (match("[")) { index = parseExpression(); consume("]", "expected ']' after index"); }
    consume("=", "expected '=' in for assignment");
    ExprPtr value = parseExpression();
    return std::make_unique<Assign>(target.line, target.text, std::move(index), std::move(value));
}
}
