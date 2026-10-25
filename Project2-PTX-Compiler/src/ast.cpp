#include "ast.hpp"

#include <iomanip>
#include <sstream>

namespace tinyptx {
bool sameType(const Type& a, const Type& b) { return a.base == b.base && a.pointer == b.pointer && a.boolean == b.boolean; }
std::string typeName(const Type& type) {
    if (type.boolean) return "bool";
    std::string result = type.base == Type::Base::Int ? "int" : "float";
    if (type.pointer) result += '*';
    return result;
}
namespace {
void indent(std::ostringstream& out, int depth) { for (int i = 0; i < depth; ++i) out << "  "; }
void dumpExpr(std::ostringstream& out, const Expr& expr, int depth) {
    indent(out, depth);
    if (const auto* integer = dynamic_cast<const IntLiteral*>(&expr)) out << "Int " << integer->value;
    else if (const auto* floating = dynamic_cast<const FloatLiteral*>(&expr)) out << "Float " << std::setprecision(8) << floating->value;
    else if (const auto* variable = dynamic_cast<const VarRef*>(&expr)) out << "Variable " << variable->name;
    else if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) { out << "Index " << index->name << "\n"; dumpExpr(out, *index->index, depth + 1); }
    else if (const auto* builtin = dynamic_cast<const BuiltinExpr*>(&expr)) out << (builtin->kind == BuiltinExpr::Kind::Gid ? "Builtin gid" : "Builtin tid");
    else if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) { out << "Unary " << unary->op << "\n"; dumpExpr(out, *unary->operand, depth + 1); }
    else if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) { out << "Binary " << binary->op << "\n"; dumpExpr(out, *binary->left, depth + 1); out << '\n'; dumpExpr(out, *binary->right, depth + 1); }
    out << " : " << typeName(expr.inferred);
}
void dumpStmt(std::ostringstream& out, const Stmt& stmt, int depth) {
    indent(out, depth);
    if (const auto* block = dynamic_cast<const Block*>(&stmt)) {
        out << "Block";
        for (const auto& child : block->statements) { out << '\n'; dumpStmt(out, *child, depth + 1); }
    } else if (const auto* declaration = dynamic_cast<const VarDecl*>(&stmt)) {
        out << "Declare " << typeName(declaration->type) << ' ' << declaration->name;
        if (declaration->initializer) { out << "\n"; dumpExpr(out, *declaration->initializer, depth + 1); }
    } else if (const auto* assignment = dynamic_cast<const Assign*>(&stmt)) {
        out << "Assign " << assignment->name;
        if (assignment->index) { out << "[\n"; dumpExpr(out, *assignment->index, depth + 1); out << '\n'; indent(out, depth); out << ']'; }
        out << "\n"; dumpExpr(out, *assignment->value, depth + 1);
    } else if (const auto* branch = dynamic_cast<const IfStmt*>(&stmt)) {
        out << "If\n"; dumpExpr(out, *branch->condition, depth + 1); out << '\n'; dumpStmt(out, *branch->thenBranch, depth + 1);
        if (branch->elseBranch) { out << '\n'; indent(out, depth); out << "Else\n"; dumpStmt(out, *branch->elseBranch, depth + 1); }
    } else if (const auto* whileLoop = dynamic_cast<const WhileStmt*>(&stmt)) {
        out << "While\n"; dumpExpr(out, *whileLoop->condition, depth + 1); out << '\n'; dumpStmt(out, *whileLoop->body, depth + 1);
    } else if (const auto* forLoop = dynamic_cast<const ForStmt*>(&stmt)) {
        out << "For";
        if (forLoop->initializer) { out << "\n"; dumpStmt(out, *forLoop->initializer, depth + 1); }
        if (forLoop->condition) { out << "\n"; dumpExpr(out, *forLoop->condition, depth + 1); }
        if (forLoop->step) { out << "\n"; dumpStmt(out, *forLoop->step, depth + 1); }
        if (forLoop->body) { out << "\n"; dumpStmt(out, *forLoop->body, depth + 1); }
    }
}
}
std::string dumpAst(const Program& program) {
    std::ostringstream out;
    for (size_t i = 0; i < program.kernels.size(); ++i) {
        const auto& k = program.kernels[i]; out << "Kernel " << k.name << '(';
        for (size_t j = 0; j < k.params.size(); ++j) { if (j) out << ", "; out << typeName(k.params[j].type) << ' ' << k.params[j].name; }
        out << ")\n"; dumpStmt(out, *k.body, 1); if (i + 1 < program.kernels.size()) out << '\n';
    }
    return out.str();
}
}
