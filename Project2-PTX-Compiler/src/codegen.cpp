#include "codegen.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tinyptx {
namespace {
struct Binding { Type type; std::string reg; };
struct Value { Type type; std::string reg; };
std::string scalarOpcodeType(const Type& t) { return t.base == Type::Base::Int ? "s32" : "f32"; }
std::string moveType(const Type& t) { return t.pointer ? "b64" : (t.base == Type::Base::Int ? "b32" : "f32"); }
std::string paramType(const Type& t) { return t.pointer ? "u64" : (t.base == Type::Base::Int ? "u32" : "f32"); }
class Generator {
public:
    std::string run(const Program& program) {
        std::ostringstream output;
        output << ".version 7.8\n.target sm_75\n.address_size 64\n\n";
        for (const auto& kernel : program.kernels) output << generateKernel(kernel) << '\n';
        return output.str();
    }
private:
    std::ostringstream body_;
    std::vector<std::map<std::string, Binding>> scopes_;
    int nextInt_ = 1, nextFloat_ = 1, nextAddress_ = 1, nextPredicate_ = 1, nextLabel_ = 1;

    void reset(const Kernel& kernel) {
        body_.str(""); body_.clear(); scopes_.clear(); scopes_.emplace_back();
        nextInt_ = nextFloat_ = nextAddress_ = nextPredicate_ = nextLabel_ = 1;
        for (const auto& param : kernel.params) {
            const std::string reg = allocate(param.type);
            scopes_.back()[param.name] = {param.type, reg};
        }
    }
    std::string allocate(const Type& type) {
        if (type.boolean) return "%p" + std::to_string(nextPredicate_++);
        if (type.pointer) return "%rd" + std::to_string(nextAddress_++);
        if (type.base == Type::Base::Float) return "%f" + std::to_string(nextFloat_++);
        return "%r" + std::to_string(nextInt_++);
    }
    Binding lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw std::runtime_error("internal code generation error: unknown variable '" + name + "'");
    }
    void pushScope() { scopes_.emplace_back(); }
    void popScope() { scopes_.pop_back(); }
    void emitMove(const std::string& dest, const std::string& src, const Type& type) { body_ << "    mov." << moveType(type) << ' ' << dest << ", " << src << ";\n"; }
    std::string generateKernel(const Kernel& kernel) {
        reset(kernel);
        size_t index = 0;
        for (const auto& param : kernel.params) {
            const auto binding = scopes_.back().at(param.name);
            const std::string loadedReg = param.type.pointer ? allocate(param.type) : binding.reg;
            body_ << "    ld.param." << paramType(param.type) << ' ' << loadedReg << ", [" << kernel.name << "_param_" << index << "];\n";
            if (param.type.pointer) body_ << "    cvta.to.global.u64 " << binding.reg << ", " << loadedReg << ";\n";
            ++index;
        }
        emitBlock(*kernel.body, false);
        body_ << "    ret;\n";
        std::ostringstream out;
        out << ".visible .entry " << kernel.name << "(\n";
        for (size_t i = 0; i < kernel.params.size(); ++i) {
            out << "    .param ." << paramType(kernel.params[i].type) << ' ' << kernel.name << "_param_" << i;
            out << (i + 1 == kernel.params.size() ? "\n" : ",\n");
        }
        out << ")\n{\n";
        if (nextPredicate_ > 1) out << "    .reg .pred %p<" << nextPredicate_ << ">;\n";
        if (nextInt_ > 1) out << "    .reg .b32 %r<" << nextInt_ << ">;\n";
        if (nextFloat_ > 1) out << "    .reg .f32 %f<" << nextFloat_ << ">;\n";
        if (nextAddress_ > 1) out << "    .reg .b64 %rd<" << nextAddress_ << ">;\n";
        out << body_.str() << "}\n";
        return out.str();
    }
    Value emitExpr(const Expr& expr) {
        if (const auto* e = dynamic_cast<const IntLiteral*>(&expr)) {
            const std::string dest = allocate(e->inferred); body_ << "    mov.s32 " << dest << ", " << e->value << ";\n"; return {e->inferred, dest};
        }
        if (const auto* e = dynamic_cast<const FloatLiteral*>(&expr)) {
            uint32_t bits = 0; std::memcpy(&bits, &e->value, sizeof(bits));
            std::ostringstream immediate; immediate << "0f" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << bits;
            const std::string dest = allocate(e->inferred); body_ << "    mov.f32 " << dest << ", " << immediate.str() << ";\n"; return {e->inferred, dest};
        }
        if (const auto* e = dynamic_cast<const VarRef*>(&expr)) { const auto b = lookup(e->name); return {b.type, b.reg}; }
        if (const auto* e = dynamic_cast<const BuiltinExpr*>(&expr)) {
            if (e->kind == BuiltinExpr::Kind::Tid) {
                const std::string dest = allocate(e->inferred); body_ << "    mov.u32 " << dest << ", %tid.x;\n"; return {e->inferred, dest};
            }
            const std::string cta = allocate(e->inferred), ntid = allocate(e->inferred), tid = allocate(e->inferred), dest = allocate(e->inferred);
            body_ << "    mov.u32 " << cta << ", %ctaid.x;\n    mov.u32 " << ntid << ", %ntid.x;\n    mov.u32 " << tid << ", %tid.x;\n";
            body_ << "    mad.lo.s32 " << dest << ", " << cta << ", " << ntid << ", " << tid << ";\n";
            return {e->inferred, dest};
        }
        if (const auto* e = dynamic_cast<const IndexExpr*>(&expr)) {
            const auto base = lookup(e->name); const Value offset = emitExpr(*e->index); const std::string address = allocate(Type{Type::Base::Int, true, false});
            const std::string byteOffset = allocate(Type{Type::Base::Int, true, false});
            body_ << "    mul.wide.s32 " << byteOffset << ", " << offset.reg << ", 4;\n";
            body_ << "    add.s64 " << address << ", " << base.reg << ", " << byteOffset << ";\n";
            const std::string dest = allocate(e->inferred);
            body_ << "    ld.global." << scalarOpcodeType(e->inferred) << ' ' << dest << ", [" << address << "];\n";
            return {e->inferred, dest};
        }
        if (const auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
            const Value value = emitExpr(*e->operand); const std::string dest = allocate(e->inferred);
            body_ << "    neg." << scalarOpcodeType(e->inferred) << ' ' << dest << ", " << value.reg << ";\n"; return {e->inferred, dest};
        }
        if (const auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
            if (e->op == "&&" || e->op == "||") {
                const Value left = emitExpr(*e->left);
                const std::string dest = allocate(e->inferred);
                const int label = nextLabel_++;
                const std::string shortLabel = "L_logic_short" + std::to_string(label), endLabel = "L_logic_end" + std::to_string(label);
                const bool isAnd = e->op == "&&";
                body_ << "    @" << (isAnd ? "!" : "") << left.reg << " bra " << shortLabel << ";\n";
                const Value right = emitExpr(*e->right);
                body_ << "    mov.pred " << dest << ", " << right.reg << ";\n";
                body_ << "    bra " << endLabel << ";\n" << shortLabel << ":\n";
                body_ << "    mov.pred " << dest << ", " << left.reg << ";\n" << endLabel << ":\n";
                return {e->inferred, dest};
            }
            const Value left = emitExpr(*e->left), right = emitExpr(*e->right);
            const std::string dest = allocate(e->inferred);
            if (e->op == "<" || e->op == "<=" || e->op == ">" || e->op == ">=" || e->op == "==" || e->op == "!=") {
                const std::map<std::string, std::string> cond{{"<", "lt"}, {"<=", "le"}, {">", "gt"}, {">=", "ge"}, {"==", "eq"}, {"!=", "ne"}};
                body_ << "    setp." << cond.at(e->op) << '.' << scalarOpcodeType(e->left->inferred) << ' ' << dest << ", " << left.reg << ", " << right.reg << ";\n";
            } else {
                std::string op;
                if (e->op == "+") op = "add.";
                else if (e->op == "-") op = "sub.";
                else if (e->op == "*") op = e->inferred.base == Type::Base::Int ? "mul.lo." : "mul.";
                else op = e->inferred.base == Type::Base::Int ? "div." : "div.rn.";
                body_ << "    " << op << scalarOpcodeType(e->inferred) << ' ' << dest << ", " << left.reg << ", " << right.reg << ";\n";
            }
            return {e->inferred, dest};
        }
        throw std::runtime_error("internal code generation error: unknown expression node");
    }
    void emitAddress(const std::string& name, const Expr& index, const Type& elementType, const std::string& valueReg, bool store) {
        const auto base = lookup(name); const Value offset = emitExpr(index);
        const std::string byteOffset = allocate(Type{Type::Base::Int, true, false});
        const std::string address = allocate(Type{Type::Base::Int, true, false});
        body_ << "    mul.wide.s32 " << byteOffset << ", " << offset.reg << ", 4;\n";
        body_ << "    add.s64 " << address << ", " << base.reg << ", " << byteOffset << ";\n";
        if (store) body_ << "    st.global." << scalarOpcodeType(elementType) << " [" << address << "], " << valueReg << ";\n";
    }
    void emitStatement(const Stmt& stmt) {
        if (const auto* block = dynamic_cast<const Block*>(&stmt)) { emitBlock(*block, true); return; }
        if (const auto* decl = dynamic_cast<const VarDecl*>(&stmt)) {
            const std::string reg = allocate(decl->type); scopes_.back()[decl->name] = {decl->type, reg};
            if (decl->initializer) { const Value value = emitExpr(*decl->initializer); emitMove(reg, value.reg, decl->type); }
            return;
        }
        if (const auto* assign = dynamic_cast<const Assign*>(&stmt)) {
            const Value value = emitExpr(*assign->value);
            if (assign->index) emitAddress(assign->name, *assign->index, assign->value->inferred, value.reg, true);
            else { const auto target = lookup(assign->name); emitMove(target.reg, value.reg, target.type); }
            return;
        }
        if (const auto* branch = dynamic_cast<const IfStmt*>(&stmt)) {
            const Value condition = emitExpr(*branch->condition);
            const int label = nextLabel_++;
            const std::string elseLabel = "L_else" + std::to_string(label), endLabel = "L_end" + std::to_string(label);
            body_ << "    @!" << condition.reg << " bra " << (branch->elseBranch ? elseLabel : endLabel) << ";\n";
            pushScope(); emitStatement(*branch->thenBranch); popScope();
            if (branch->elseBranch) {
                body_ << "    bra " << endLabel << ";\n" << elseLabel << ":\n";
                pushScope(); emitStatement(*branch->elseBranch); popScope();
            }
            body_ << endLabel << ":\n";
            return;
        }
        if (const auto* loop = dynamic_cast<const WhileStmt*>(&stmt)) {
            const int label = nextLabel_++;
            const std::string condLabel = "L_cond" + std::to_string(label), endLabel = "L_end" + std::to_string(label);
            body_ << condLabel << ":\n";
            const Value condition = emitExpr(*loop->condition);
            body_ << "    @!" << condition.reg << " bra " << endLabel << ";\n";
            pushScope(); emitStatement(*loop->body); popScope();
            body_ << "    bra " << condLabel << ";\n" << endLabel << ":\n";
            return;
        }
        if (const auto* loop = dynamic_cast<const ForStmt*>(&stmt)) {
            pushScope();
            if (loop->initializer) emitStatement(*loop->initializer);
            const int label = nextLabel_++;
            const std::string condLabel = "L_cond" + std::to_string(label), endLabel = "L_end" + std::to_string(label);
            body_ << condLabel << ":\n";
            if (loop->condition) {
                const Value condition = emitExpr(*loop->condition);
                body_ << "    @!" << condition.reg << " bra " << endLabel << ";\n";
            }
            if (loop->body) { pushScope(); emitStatement(*loop->body); popScope(); }
            if (loop->step) emitStatement(*loop->step);
            body_ << "    bra " << condLabel << ";\n" << endLabel << ":\n";
            popScope();
            return;
        }
    }
    void emitBlock(const Block& block, bool newScope) {
        if (newScope) pushScope();
        for (const auto& stmt : block.statements) emitStatement(*stmt);
        if (newScope) popScope();
    }
};
}
std::string generatePTX(const Program& program) { return Generator{}.run(program); }
}
