#include "analyzer.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ptx {
namespace {
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string baseOpcode(const std::string& opcode) {
    const auto dot = opcode.find('.');
    return lower(opcode.substr(0, dot));
}
bool isTerminator(const Instruction& i) {
    const auto op = baseOpcode(i.opcode);
    return op == "bra" || op == "ret" || op == "exit" || op == "kill" || op == "trap";
}
bool isBranch(const Instruction& i) { return baseOpcode(i.opcode) == "bra"; }
std::string dotEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        if (c != '\r') out.push_back(c);
    }
    return out;
}
std::vector<std::string> registersIn(const std::string& s) {
    std::vector<std::string> regs;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '%' || i + 1 >= s.size() || !(std::isalpha(static_cast<unsigned char>(s[i + 1])) || s[i + 1] == '_')) continue;
        size_t end = i + 2;
        while (end < s.size()) {
            const unsigned char c = static_cast<unsigned char>(s[end]);
            if (!std::isalnum(c) && s[end] != '_' && s[end] != '$' && s[end] != '.') break;
            ++end;
        }
        const std::string reg = s.substr(i, end - i);
        const std::string name = lower(reg);
        const char* special[] = {"%tid.", "%ntid.", "%ctaid.", "%nctaid.", "%laneid", "%warpid", "%smid", "%gridid", "%lanemask", "%clock", "%globaltimer", "%envreg", "%reserved_smem"};
        bool builtin = false;
        for (const char* prefix : special) if (name.find(prefix) == 0) { builtin = true; break; }
        if (!builtin && std::find(regs.begin(), regs.end(), reg) == regs.end()) regs.push_back(reg);
        i = end - 1;
    }
    return regs;
}
std::vector<std::string> splitOperands(const std::string& operands) {
    std::vector<std::string> result;
    size_t start = 0;
    int nesting = 0;
    for (size_t i = 0; i < operands.size(); ++i) {
        const char c = operands[i];
        if (c == '[' || c == '{' || c == '(') ++nesting;
        else if (c == ']' || c == '}' || c == ')') --nesting;
        else if (c == ',' && nesting == 0) { result.push_back(trim(operands.substr(start, i - start))); start = i + 1; }
    }
    if (start < operands.size()) result.push_back(trim(operands.substr(start)));
    return result;
}
Instruction makeInstruction(const std::string& statement, int line, int functionId) {
    Instruction inst;
    inst.source = statement;
    inst.line = line;
    inst.functionId = functionId;
    std::string parsed = trim(statement);
    if (!parsed.empty() && parsed[0] == '@') {
        const auto ws = parsed.find_first_of(" \t");
        if (ws != std::string::npos) { inst.predicate = parsed.substr(0, ws); parsed = trim(parsed.substr(ws + 1)); }
    }
    const auto ws = parsed.find_first_of(" \t");
    inst.opcode = ws == std::string::npos ? parsed : parsed.substr(0, ws);
    inst.operands = ws == std::string::npos ? "" : trim(parsed.substr(ws + 1));

    const auto chunks = splitOperands(inst.operands);
    const std::string op = baseOpcode(inst.opcode);
    const bool noDestination = op == "st" || op == "bra" || op == "ret" || op == "exit" || op == "kill" || op == "trap" || op == "bar" || op == "membar" || op == "fence" || op == "prefetch" || op == "prefetchu";
    if (!inst.predicate.empty()) inst.uses = registersIn(inst.predicate);
    if (!noDestination && !chunks.empty()) inst.definitions = registersIn(chunks.front());
    const size_t firstUse = (!noDestination && !chunks.empty()) ? 1 : 0;
    for (size_t i = firstUse; i < chunks.size(); ++i) {
        const auto regs = registersIn(chunks[i]);
        for (const auto& reg : regs) if (std::find(inst.uses.begin(), inst.uses.end(), reg) == inst.uses.end()) inst.uses.push_back(reg);
    }
    return inst;
}
std::string branchTarget(const Instruction& i) {
    std::string s = trim(i.operands);
    const auto comma = s.rfind(',');
    if (comma != std::string::npos) s = trim(s.substr(comma + 1));
    const auto space = s.find_last_of(" \t");
    if (space != std::string::npos) s = s.substr(space + 1);
    while (!s.empty() && (s.back() == ';' || s.back() == ',')) s.pop_back();
    return s;
}
void addEdge(Program& p, int from, int to) {
    if (from < 0 || to < 0) return;
    auto& succ = p.blocks[from].successors;
    if (std::find(succ.begin(), succ.end(), to) == succ.end()) succ.push_back(to);
    auto& pred = p.blocks[to].predecessors;
    if (std::find(pred.begin(), pred.end(), from) == pred.end()) pred.push_back(from);
}
void computeAnalyses(Program& p) {
    const int n = static_cast<int>(p.blocks.size());
    p.dominators.assign(n, {});
    p.reachableBlocks.assign(n, false);
    p.loops.clear();
    p.liveIn.assign(n, {});
    p.liveOut.assign(n, {});
    p.maxPressure = 0;
    p.maxPressureBlock = -1;
    if (n == 0) return;

    std::vector<int> entries;
    for (const auto& b : p.blocks) if (b.isEntry) entries.push_back(b.id);
    if (entries.empty()) { p.blocks[0].isEntry = true; entries.push_back(0); }
    std::vector<int> work = entries;
    for (int e : entries) p.reachableBlocks[e] = true;
    while (!work.empty()) {
        const int b = work.back(); work.pop_back();
        for (int s : p.blocks[b].successors) if (!p.reachableBlocks[s]) { p.reachableBlocks[s] = true; work.push_back(s); }
    }

    std::map<int, std::set<int>> functionBlocks;
    for (const auto& b : p.blocks) {
        if (p.reachableBlocks[b.id]) functionBlocks[b.functionId].insert(b.id);
    }
    for (const auto& b : p.blocks) {
        if (!p.reachableBlocks[b.id] || b.isEntry) p.dominators[b.id] = {b.id};
        else p.dominators[b.id].assign(functionBlocks[b.functionId].begin(), functionBlocks[b.functionId].end());
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& b : p.blocks) {
            if (!p.reachableBlocks[b.id] || b.isEntry) continue;
            std::set<int> intersection = functionBlocks[b.functionId];
            bool hasReachablePred = false;
            for (int pred : b.predecessors) {
                if (!p.reachableBlocks[pred] || p.blocks[pred].functionId != b.functionId) continue;
                std::set<int> predDom(p.dominators[pred].begin(), p.dominators[pred].end()), next;
                std::set_intersection(intersection.begin(), intersection.end(), predDom.begin(), predDom.end(), std::inserter(next, next.begin()));
                intersection.swap(next);
                hasReachablePred = true;
            }
            if (!hasReachablePred) intersection.clear();
            intersection.insert(b.id);
            std::vector<int> updated(intersection.begin(), intersection.end());
            if (updated != p.dominators[b.id]) { p.dominators[b.id] = updated; changed = true; }
        }
    }
    for (const auto& latch : p.blocks) for (int header : latch.successors) {
        if (latch.functionId != p.blocks[header].functionId || !p.reachableBlocks[latch.id]) continue;
        if (std::find(p.dominators[latch.id].begin(), p.dominators[latch.id].end(), header) == p.dominators[latch.id].end()) continue;
        std::set<int> loop{header, latch.id};
        std::vector<int> loopWork{latch.id};
        while (!loopWork.empty()) {
            const int x = loopWork.back(); loopWork.pop_back();
            if (x == header) continue;
            for (int pred : p.blocks[x].predecessors) {
                if (p.blocks[pred].functionId == latch.functionId && loop.insert(pred).second) loopWork.push_back(pred);
            }
        }
        NaturalLoop found; found.header = header; found.latch = latch.id;
        found.blocks.assign(loop.begin(), loop.end()); p.loops.push_back(found);
    }

    std::vector<std::set<std::string>> blockUse(n), blockDef(n);
    for (const auto& b : p.blocks) {
        for (int ix : b.instructions) {
            const auto& inst = p.instructions[ix];
            for (const auto& reg : inst.uses) if (blockDef[b.id].count(reg) == 0) blockUse[b.id].insert(reg);
            blockDef[b.id].insert(inst.definitions.begin(), inst.definitions.end());
        }
    }
    changed = true;
    while (changed) {
        changed = false;
        for (int b = n - 1; b >= 0; --b) {
            std::set<std::string> out;
            for (int succ : p.blocks[b].successors) out.insert(p.liveIn[succ].begin(), p.liveIn[succ].end());
            std::set<std::string> in = blockUse[b];
            for (const auto& reg : out) if (blockDef[b].count(reg) == 0) in.insert(reg);
            if (out != p.liveOut[b] || in != p.liveIn[b]) { p.liveOut[b].swap(out); p.liveIn[b].swap(in); changed = true; }
        }
    }
    for (const auto& b : p.blocks) {
        if (!p.reachableBlocks[b.id]) continue;
        std::set<std::string> live = p.liveOut[b.id];
        if (static_cast<int>(live.size()) > p.maxPressure) { p.maxPressure = static_cast<int>(live.size()); p.maxPressureBlock = b.id; }
        for (auto it = b.instructions.rbegin(); it != b.instructions.rend(); ++it) {
            const auto& inst = p.instructions[*it];
            for (const auto& reg : inst.definitions) live.erase(reg);
            live.insert(inst.uses.begin(), inst.uses.end());
            if (static_cast<int>(live.size()) > p.maxPressure) { p.maxPressure = static_cast<int>(live.size()); p.maxPressureBlock = b.id; }
        }
    }
}

std::string formatSet(const std::set<std::string>& values) {
    std::ostringstream out; out << '{'; bool first = true;
    for (const auto& value : values) { if (!first) out << ", "; out << value; first = false; }
    out << '}'; return out.str();
}
}

Program analyze(const std::string& source) {
    Program p;
    std::map<int, std::vector<std::string>> labelsAt;
    std::vector<int> functionStarts;
    std::istringstream input(source);
    std::string line;
    int lineNo = 0;
    int currentFunction = 0;
    bool sawFunction = false;
    while (std::getline(input, line)) {
        ++lineNo;
        const auto comment = line.find("//");
        if (comment != std::string::npos) line.erase(comment);
        const std::string cleanLine = trim(line);
        const std::string lowerLine = lower(cleanLine);
        size_t marker = lowerLine.find(".entry");
        size_t markerLength = 6;
        if (marker == std::string::npos) { marker = lowerLine.find(".func"); markerLength = 5; }
        if (marker != std::string::npos && (marker == 0 || std::isspace(static_cast<unsigned char>(lowerLine[marker - 1])))) {
            std::string tail = trim(cleanLine.substr(marker + markerLength));
            std::string name;
            if (tail.size() >= 2 && tail[0] == '(') {
                const auto close = tail.find(')');
                if (close != std::string::npos) tail = trim(tail.substr(close + 1));
            }
            const auto endName = tail.find_first_of(" (\t\r\n");
            name = endName == std::string::npos ? tail : tail.substr(0, endName);
            if (name.empty()) name = "function" + std::to_string(p.functions.size());
            currentFunction = static_cast<int>(p.functions.size());
            p.functions.push_back({currentFunction, name, static_cast<int>(p.instructions.size())});
            functionStarts.push_back(static_cast<int>(p.instructions.size()));
            sawFunction = true;
        }
        std::stringstream pieces(line);
        std::string piece;
        while (std::getline(pieces, piece, ';')) {
            piece = trim(piece);
            while (!piece.empty()) {
                while (!piece.empty() && piece[0] == '{') piece = trim(piece.substr(1));
                if (piece.empty() || piece[0] == '}') break;
                if (piece == ")" || piece == ") {") break;
                const auto colon = piece.find(':');
                const auto firstSpace = piece.find_first_of(" \t");
                if (colon != std::string::npos && (firstSpace == std::string::npos || colon < firstSpace)) {
                    const auto label = trim(piece.substr(0, colon));
                    if (!label.empty()) labelsAt[static_cast<int>(p.instructions.size())].push_back(label);
                    piece = trim(piece.substr(colon + 1));
                    if (piece.empty()) break;
                }
                if (piece[0] == '.') break; // Directives and declarations are not executable instructions.
                auto inst = makeInstruction(piece, lineNo, currentFunction);
                if (!inst.opcode.empty()) { p.instructions.push_back(inst); p.instructionFunctions.push_back(currentFunction); }
                break;
            }
        }
    }
    if (p.instructions.empty()) return p;
    if (!sawFunction) {
        p.functions.push_back({0, "input", 0});
        functionStarts.push_back(0);
        std::fill(p.instructionFunctions.begin(), p.instructionFunctions.end(), 0);
        for (auto& inst : p.instructions) inst.functionId = 0;
    }

    std::map<std::pair<int, std::string>, int> labelInstruction;
    for (const auto& entry : labelsAt) {
        if (entry.first >= static_cast<int>(p.instructions.size())) continue;
        const int functionId = p.instructionFunctions[entry.first];
        for (const auto& label : entry.second) labelInstruction[{functionId, label}] = entry.first;
    }
    std::set<int> leaders{0};
    for (int start : functionStarts) if (start < static_cast<int>(p.instructions.size())) leaders.insert(start);
    for (const auto& entry : labelsAt) if (entry.first < static_cast<int>(p.instructions.size())) leaders.insert(entry.first);
    for (int i = 0; i < static_cast<int>(p.instructions.size()); ++i) {
        const auto& inst = p.instructions[i];
        if (baseOpcode(inst.opcode) == "brx") throw std::runtime_error("line " + std::to_string(inst.line) + ": indirect brx is not supported");
        if (isBranch(inst)) {
            const auto target = labelInstruction.find({inst.functionId, branchTarget(inst)});
            if (target != labelInstruction.end()) leaders.insert(target->second);
        }
        if (isTerminator(inst) && i + 1 < static_cast<int>(p.instructions.size()) && p.instructionFunctions[i + 1] == inst.functionId) leaders.insert(i + 1);
    }
    std::vector<int> starts(leaders.begin(), leaders.end());
    std::vector<int> instructionBlock(p.instructions.size(), -1);
    for (size_t b = 0; b < starts.size(); ++b) {
        BasicBlock block; block.id = static_cast<int>(b); block.functionId = p.instructionFunctions[starts[b]];
        block.isEntry = std::find(functionStarts.begin(), functionStarts.end(), starts[b]) != functionStarts.end();
        const int end = b + 1 < starts.size() ? starts[b + 1] : static_cast<int>(p.instructions.size());
        for (int i = starts[b]; i < end; ++i) { block.instructions.push_back(i); instructionBlock[i] = block.id; }
        const auto foundLabels = labelsAt.find(starts[b]);
        if (foundLabels != labelsAt.end()) block.labels = foundLabels->second;
        p.blocks.push_back(block);
    }
    for (int b = 0; b < static_cast<int>(p.blocks.size()); ++b) {
        const auto& last = p.instructions[p.blocks[b].instructions.back()];
        const int next = b + 1 < static_cast<int>(p.blocks.size()) && p.blocks[b + 1].functionId == p.blocks[b].functionId ? b + 1 : -1;
        if (isBranch(last)) {
            const auto target = labelInstruction.find({last.functionId, branchTarget(last)});
            if (target == labelInstruction.end()) throw std::runtime_error("line " + std::to_string(last.line) + ": unknown branch target '" + branchTarget(last) + "'");
            addEdge(p, b, instructionBlock[target->second]);
            if (!last.predicate.empty()) addEdge(p, b, next);
        } else if (isTerminator(last)) {
            if (!last.predicate.empty()) addEdge(p, b, next);
        } else if (next >= 0) addEdge(p, b, next);
    }
    computeAnalyses(p);
    return p;
}

Program removeUnreachable(const Program& program) {
    Program out = program;
    std::vector<int> oldToNew(program.blocks.size(), -1);
    out.blocks.clear();
    for (const auto& block : program.blocks) if (block.id < static_cast<int>(program.reachableBlocks.size()) && program.reachableBlocks[block.id]) {
        BasicBlock copy = block;
        copy.id = static_cast<int>(out.blocks.size());
        copy.successors.clear(); copy.predecessors.clear();
        oldToNew[block.id] = copy.id;
        out.blocks.push_back(copy);
    }
    for (const auto& old : program.blocks) {
        if (oldToNew[old.id] < 0) continue;
        auto& copy = out.blocks[oldToNew[old.id]];
        for (int succ : old.successors) if (oldToNew[succ] >= 0) copy.successors.push_back(oldToNew[succ]);
    }
    for (auto& block : out.blocks) for (int succ : block.successors) out.blocks[succ].predecessors.push_back(block.id);
    computeAnalyses(out);
    return out;
}
int activeInstructionCount(const Program& program) {
    int count = 0; for (const auto& b : program.blocks) count += static_cast<int>(b.instructions.size()); return count;
}

std::string toDot(const Program& p) {
    std::ostringstream out; out << "digraph CFG {\n  node [shape=box];\n";
    for (const auto& b : p.blocks) {
        std::ostringstream label; label << "B" << b.id << " (function " << b.functionId << ')';
        if (!b.labels.empty()) { label << '\n'; for (size_t i = 0; i < b.labels.size(); ++i) { if (i) label << ", "; label << b.labels[i]; } }
        for (int ix : b.instructions) label << '\n' << p.instructions[ix].opcode << " " << p.instructions[ix].operands;
        out << "  B" << b.id << " [label=\"" << dotEscape(label.str()) << "\"];\n";
    }
    for (const auto& b : p.blocks) for (int s : b.successors) out << "  B" << b.id << " -> B" << s << ";\n";
    out << "}\n"; return out.str();
}
std::string dominatorText(const Program& p) {
    std::ostringstream out;
    for (const auto& b : p.blocks) {
        out << "B" << b.id << " (function " << b.functionId << "): ";
        for (size_t i = 0; i < p.dominators[b.id].size(); ++i) { if (i) out << ", "; out << "B" << p.dominators[b.id][i]; }
        out << '\n';
    }
    return out.str();
}
std::string loopText(const Program& p) {
    std::ostringstream out;
    for (size_t i = 0; i < p.loops.size(); ++i) {
        const auto& loop = p.loops[i]; out << "Loop " << i + 1 << ": header B" << loop.header << ", latch B" << loop.latch << ", blocks {";
        for (size_t j = 0; j < loop.blocks.size(); ++j) { if (j) out << ", "; out << 'B' << loop.blocks[j]; }
        out << "}\n";
    }
    if (p.loops.empty()) out << "No natural loops found.\n";
    return out.str();
}
std::string livenessText(const Program& p) {
    std::ostringstream out;
    for (const auto& b : p.blocks) out << "B" << b.id << " IN=" << formatSet(p.liveIn[b.id]) << " OUT=" << formatSet(p.liveOut[b.id]) << '\n';
    if (p.maxPressureBlock >= 0) out << "Estimated peak live-register pressure: " << p.maxPressure << " registers in B" << p.maxPressureBlock << " (PTX virtual registers; built-in special registers excluded)\n";
    else out << "Estimated peak live-register pressure: 0 registers\n";
    return out.str();
}
}
