#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tinyptx {
struct Type {
    enum class Base { Int, Float } base = Base::Int;
    bool pointer = false;
    bool boolean = false; // Internal expression type produced by comparisons.
};
bool sameType(const Type& a, const Type& b);
std::string typeName(const Type& type);

struct Expr {
    explicit Expr(int sourceLine) : line(sourceLine) {}
    virtual ~Expr() = default;
    int line;
    Type inferred;
};
using ExprPtr = std::unique_ptr<Expr>;
struct IntLiteral final : Expr { IntLiteral(int line, int32_t v) : Expr(line), value(v) {} int32_t value; };
struct FloatLiteral final : Expr { FloatLiteral(int line, float v) : Expr(line), value(v) {} float value; };
struct VarRef final : Expr { VarRef(int line, std::string n) : Expr(line), name(std::move(n)) {} std::string name; };
struct IndexExpr final : Expr { IndexExpr(int line, std::string n, ExprPtr i) : Expr(line), name(std::move(n)), index(std::move(i)) {} std::string name; ExprPtr index; };
struct BuiltinExpr final : Expr { enum class Kind { Gid, Tid }; BuiltinExpr(int line, Kind k) : Expr(line), kind(k) {} Kind kind; };
struct UnaryExpr final : Expr { UnaryExpr(int line, std::string o, ExprPtr e) : Expr(line), op(std::move(o)), operand(std::move(e)) {} std::string op; ExprPtr operand; };
struct BinaryExpr final : Expr { BinaryExpr(int line, std::string o, ExprPtr l, ExprPtr r) : Expr(line), op(std::move(o)), left(std::move(l)), right(std::move(r)) {} std::string op; ExprPtr left; ExprPtr right; };

struct Stmt { explicit Stmt(int sourceLine) : line(sourceLine) {} virtual ~Stmt() = default; int line; };
using StmtPtr = std::unique_ptr<Stmt>;
struct Block final : Stmt { explicit Block(int line) : Stmt(line) {} std::vector<StmtPtr> statements; };
struct VarDecl final : Stmt { VarDecl(int line, Type t, std::string n, ExprPtr i) : Stmt(line), type(t), name(std::move(n)), initializer(std::move(i)) {} Type type; std::string name; ExprPtr initializer; };
struct Assign final : Stmt { Assign(int line, std::string n, ExprPtr i, ExprPtr v) : Stmt(line), name(std::move(n)), index(std::move(i)), value(std::move(v)) {} std::string name; ExprPtr index; ExprPtr value; };
struct IfStmt final : Stmt { IfStmt(int line, ExprPtr c, StmtPtr t, StmtPtr e) : Stmt(line), condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {} ExprPtr condition; StmtPtr thenBranch; StmtPtr elseBranch; };
struct WhileStmt final : Stmt { WhileStmt(int line, ExprPtr c, StmtPtr b) : Stmt(line), condition(std::move(c)), body(std::move(b)) {} ExprPtr condition; StmtPtr body; };
struct ForStmt final : Stmt { ForStmt(int line, StmtPtr i, ExprPtr c, StmtPtr s, StmtPtr b) : Stmt(line), initializer(std::move(i)), condition(std::move(c)), step(std::move(s)), body(std::move(b)) {} StmtPtr initializer; ExprPtr condition; StmtPtr step; StmtPtr body; };

struct Param { int line; Type type; std::string name; };
struct Kernel { int line; std::string name; std::vector<Param> params; std::unique_ptr<Block> body; };
struct Program { std::vector<Kernel> kernels; };
std::string dumpAst(const Program& program);
}
