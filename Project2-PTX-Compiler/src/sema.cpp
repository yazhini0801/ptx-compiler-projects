#include "sema.hpp"
#include "diagnostic.hpp"

#include <map>
#include <string>
#include <vector>

namespace tinyptx {
namespace {
Type scalar(Type::Base base) { return Type{base, false, false}; }
Type booleanType() { return Type{Type::Base::Int, false, true}; }
class Checker {
public:
    void check(Program& program) {
        for (auto& kernel : program.kernels) checkKernel(kernel);
    }
private:
    std::vector<std::map<std::string, Type>> scopes_;
    void pushScope() { scopes_.emplace_back(); }
    void popScope() { scopes_.pop_back(); }
    void declare(const std::string& name, const Type& type, int line) {
        auto& scope = scopes_.back();
        if (scope.count(name)) throw CompileError(line, 1, "duplicate declaration of '" + name + "' in this scope");
        scope[name] = type;
    }
    Type lookup(const std::string& name, int line) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw CompileError(line, 1, "use of undeclared variable '" + name + "'");
    }
    void checkKernel(Kernel& kernel) {
        scopes_.clear(); pushScope();
        for (const auto& param : kernel.params) declare(param.name, param.type, param.line);
        checkBlock(*kernel.body, false);
        popScope();
    }
    void checkBlock(Block& block, bool newScope) {
        if (newScope) pushScope();
        for (auto& stmt : block.statements) checkStatement(*stmt);
        if (newScope) popScope();
    }
    void checkScopedStatement(Stmt& stmt) {
        pushScope(); checkStatement(stmt); popScope();
    }
    void checkStatement(Stmt& stmt) {
        if (auto* block = dynamic_cast<Block*>(&stmt)) { checkBlock(*block, true); return; }
        if (auto* decl = dynamic_cast<VarDecl*>(&stmt)) {
            declare(decl->name, decl->type, decl->line);
            if (decl->initializer) {
                const Type value = infer(*decl->initializer);
                if (!sameType(decl->type, value)) throw CompileError(decl->line, 1, "cannot initialize " + typeName(decl->type) + " with " + typeName(value));
            }
            return;
        }
        if (auto* assign = dynamic_cast<Assign*>(&stmt)) {
            const Type target = lookup(assign->name, assign->line);
            Type expected = target;
            if (assign->index) {
                if (!target.pointer) throw CompileError(assign->line, 1, "cannot index non-pointer variable '" + assign->name + "'");
                const Type indexType = infer(*assign->index);
                if (indexType.pointer || indexType.boolean || indexType.base != Type::Base::Int) throw CompileError(assign->index->line, 1, "array index must have type int");
                expected.pointer = false;
            } else if (target.boolean) {
                throw CompileError(assign->line, 1, "a comparison result cannot be assigned to a variable");
            }
            const Type value = infer(*assign->value);
            if (!sameType(expected, value)) throw CompileError(assign->line, 1, "cannot assign " + typeName(value) + " to " + typeName(expected));
            return;
        }
        if (auto* branch = dynamic_cast<IfStmt*>(&stmt)) {
            const Type condition = infer(*branch->condition);
            requireComparison(condition, branch->condition->line);
            checkScopedStatement(*branch->thenBranch);
            if (branch->elseBranch) checkScopedStatement(*branch->elseBranch);
            return;
        }
        if (auto* loop = dynamic_cast<WhileStmt*>(&stmt)) {
            const Type condition = infer(*loop->condition);
            requireComparison(condition, loop->condition->line);
            checkScopedStatement(*loop->body);
            return;
        }
        if (auto* loop = dynamic_cast<ForStmt*>(&stmt)) {
            pushScope();
            if (loop->initializer) checkStatement(*loop->initializer);
            if (loop->condition) requireComparison(infer(*loop->condition), loop->condition->line);
            if (loop->body) checkScopedStatement(*loop->body);
            if (loop->step) checkStatement(*loop->step);
            popScope();
            return;
        }
    }
    void requireComparison(const Type& condition, int line) const {
        if (!condition.boolean) throw CompileError(line, 1, "if/while/for condition must be a comparison");
    }
    Type infer(Expr& expr) {
        if (dynamic_cast<IntLiteral*>(&expr)) return expr.inferred = scalar(Type::Base::Int);
        if (dynamic_cast<FloatLiteral*>(&expr)) return expr.inferred = scalar(Type::Base::Float);
        if (auto* var = dynamic_cast<VarRef*>(&expr)) return expr.inferred = lookup(var->name, var->line);
        if (auto* index = dynamic_cast<IndexExpr*>(&expr)) {
            const Type base = lookup(index->name, index->line);
            if (!base.pointer) throw CompileError(index->line, 1, "cannot index non-pointer variable '" + index->name + "'");
            const Type subscript = infer(*index->index);
            if (subscript.pointer || subscript.boolean || subscript.base != Type::Base::Int) throw CompileError(index->index->line, 1, "array index must have type int");
            Type element = base; element.pointer = false;
            return expr.inferred = element;
        }
        if (dynamic_cast<BuiltinExpr*>(&expr)) return expr.inferred = scalar(Type::Base::Int);
        if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
            const Type operand = infer(*unary->operand);
            if (operand.pointer || operand.boolean) throw CompileError(unary->line, 1, "unary minus requires an int or float value");
            return expr.inferred = operand;
        }
        if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
            const Type left = infer(*binary->left), right = infer(*binary->right);
            if (binary->op == "&&" || binary->op == "||") {
                if (!left.boolean || !right.boolean) throw CompileError(binary->line, 1, "logical operator '" + binary->op + "' requires comparison operands");
                return expr.inferred = booleanType();
            }
            if (!sameType(left, right)) throw CompileError(binary->line, 1, "mixed or incompatible types in '" + binary->op + "': " + typeName(left) + " and " + typeName(right));
            if (left.pointer || left.boolean) throw CompileError(binary->line, 1, "operator '" + binary->op + "' requires scalar int or float operands");
            if (binary->op == "<" || binary->op == "<=" || binary->op == ">" || binary->op == ">=" || binary->op == "==" || binary->op == "!=") return expr.inferred = booleanType();
            return expr.inferred = left;
        }
        throw CompileError(expr.line, 1, "internal error: unknown expression node");
    }
};
}
void checkSemantics(Program& program) { Checker{}.check(program); }
}
