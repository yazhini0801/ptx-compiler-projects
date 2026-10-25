#include "analyzer.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
bool hasBlock(const ptx::NaturalLoop& loop, int block) {
    return std::find(loop.blocks.begin(), loop.blocks.end(), block) != loop.blocks.end();
}
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    assert(file && "Could not open test fixture; run tests from the project root.");
    std::ostringstream contents; contents << file.rdbuf(); return contents.str();
}
}

int main() {
    const std::string singleLoop = R"(.version 7.0
.target sm_70
.visible .entry test(
  .param .u32 n
)
{
  .reg .pred %p<2>;
  .reg .b32 %r<3>;
  mov.u32 %r1, 0;
loop:
  add.u32 %r1, %r1, 1;
  setp.lt.u32 %p1, %r1, 4;
  @%p1 bra loop;
  ret;
})";
    const auto p = ptx::analyze(singleLoop);
    assert(p.instructions.size() == 5);
    assert(p.functions.size() == 1 && p.functions[0].name == "test");
    assert(p.blocks.size() == 3);
    assert(p.blocks[0].successors.size() == 1 && p.blocks[0].successors[0] == 1);
    assert(p.blocks[1].successors.size() == 2);
    assert(std::find(p.blocks[1].successors.begin(), p.blocks[1].successors.end(), 1) != p.blocks[1].successors.end());
    assert(p.blocks[2].successors.empty());
    assert(p.dominators[1].size() == 2 && p.dominators[1][0] == 0 && p.dominators[1][1] == 1);
    assert(p.loops.size() == 1 && p.loops[0].header == 1 && p.loops[0].latch == 1);
    assert(p.loops[0].blocks.size() == 1);
    assert(p.maxPressure > 0);
    const auto dot = ptx::toDot(p);
    assert(dot.find("digraph CFG") != std::string::npos);
    assert(dot.find("B1 -> B1") != std::string::npos);
    assert(dot.find("B1 -> B2") != std::string::npos);

    const auto liveRegisters = ptx::analyze(".entry live() {\nmov.u32 %r1, 1;\nadd.u32 %r2, %r1, %r3;\nst.global.u32 [%rd1], %r2;\nret;\n}\n");
    assert(liveRegisters.liveIn[0].count("%r3") == 1);
    assert(liveRegisters.liveIn[0].count("%rd1") == 1);
    assert(liveRegisters.liveIn[0].count("%r1") == 0);
    assert(liveRegisters.maxPressure >= 2);

    const std::string multiBlockLoop = R"(.entry multi_block() {
.reg .pred %p<3>;
.reg .b32 %r<2>;
mov.u32 %r1, 0;
loop:
setp.ge.u32 %p1, %r1, 4;
@%p1 bra done;
setp.eq.u32 %p2, %r1, 2;
@%p2 bra skip;
add.u32 %r1, %r1, 1;
skip:
add.u32 %r1, %r1, 1;
bra loop;
done:
ret;
})";
    const auto multi = ptx::analyze(multiBlockLoop);
    assert(multi.loops.size() == 1);
    assert(multi.loops[0].blocks.size() >= 3);
    assert(hasBlock(multi.loops[0], multi.loops[0].header));
    assert(hasBlock(multi.loops[0], multi.loops[0].latch));

    const std::string nested = R"(.entry nested() {
.reg .pred %p<3>;
.reg .b32 %r<4>;
mov.u32 %r1, 0;
outer:
setp.ge.u32 %p1, %r1, 3;
@%p1 bra done;
mov.u32 %r2, 0;
inner:
setp.ge.u32 %p2, %r2, 2;
@%p2 bra outer_latch;
add.u32 %r3, %r3, %r1;
add.u32 %r2, %r2, 1;
bra inner;
outer_latch:
add.u32 %r1, %r1, 1;
bra outer;
done:
ret;
})";
    const auto nestedProgram = ptx::analyze(nested);
    assert(nestedProgram.loops.size() == 2);
    const ptx::NaturalLoop* outerLoop = nullptr;
    const ptx::NaturalLoop* innerLoop = nullptr;
    for (const auto& loop : nestedProgram.loops) {
        if (loop.header == 1) outerLoop = &loop;
        else innerLoop = &loop;
    }
    assert(outerLoop != nullptr && innerLoop != nullptr);
    assert(outerLoop->blocks.size() > innerLoop->blocks.size());

    const auto noLoop = ptx::analyze(".entry straight() {\nmov.u32 %r1, 1;\nret;\n}\n");
    assert(noLoop.loops.empty());
    assert(ptx::loopText(noLoop).find("No natural loops found") != std::string::npos);

    const auto predicatedReturn = ptx::analyze(".entry pred_ret() {\n@%p1 ret;\nmov.u32 %r1, 1;\nret;\n}\n");
    assert(predicatedReturn.blocks[0].successors.size() == 1 && predicatedReturn.blocks[0].successors[0] == 1);

    const auto deadCode = ptx::analyze(".entry dead() {\nmov.u32 %r1, 1;\nret;\ndead_label:\nadd.u32 %r2, %r1, 1;\nret;\n}\n");
    assert(deadCode.blocks.size() == 2);
    assert(deadCode.reachableBlocks.size() == 2 && !deadCode.reachableBlocks[1]);
    const auto pruned = ptx::removeUnreachable(deadCode);
    assert(pruned.blocks.size() == 1);
    assert(ptx::activeInstructionCount(deadCode) == 4);
    assert(ptx::activeInstructionCount(pruned) == 2);

    const auto realPtx = ptx::analyze(readFile("tests/real_kernels.ptx"));
    assert(realPtx.functions.size() == 4);
    assert(realPtx.blocks.size() > 8);
    assert(realPtx.loops.size() >= 3);
    assert(realPtx.maxPressure > 0);
    for (int b = 0; b < static_cast<int>(realPtx.blocks.size()); ++b) {
        for (int succ : realPtx.blocks[b].successors) assert(realPtx.blocks[succ].functionId == realPtx.blocks[b].functionId);
    }
    bool badTarget = false;
    try { ptx::analyze(".entry bad() {\nbra missing;\n}\n"); } catch (...) { badTarget = true; }
    assert(badTarget);
    bool indirectBranchRejected = false;
    try { ptx::analyze(".entry indirect() {\nbrx.idx %r1;\n}\n"); } catch (...) { indirectBranchRejected = true; }
    assert(indirectBranchRejected);
    std::cout << "All PTX Analyzer unit and NVCC PTX integration tests passed.\n";
}
