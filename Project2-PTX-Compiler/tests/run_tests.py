#!/usr/bin/env python3
"""Compile positive and negative source fixtures through the real ptxc CLI."""
import pathlib
import argparse
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SUCCESS = [
    "examples/vadd.k",
    "examples/saxpy.k",
    "examples/sum_to.k",
    "tests/programs/if_else.k",
    "tests/programs/nested_if.k",
    "tests/programs/nested_while.k",
    "tests/programs/tid_only.k",
    "tests/programs/two_kernels.k",
    "tests/programs/float_ops.k",
    "examples/for_logical.k",
]
ERRORS = {
    "tests/programs/type_mismatch.k": "mixed or incompatible types",
    "tests/programs/undeclared.k": "undeclared variable",
    "tests/programs/duplicate.k": "duplicate declaration",
    "tests/programs/nonpointer_index.k": "cannot index non-pointer",
    "tests/programs/not_comparison.k": "condition must be a comparison",
    "tests/programs/invalid_assignment.k": "expected declaration",
    "tests/programs/missing_semicolon.k": "expected ';' after assignment",
    "tests/programs/unmatched_brace.k": "expected '}' to close block",
    "tests/programs/wrong_builtin_args.k": "built-in takes no arguments",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", nargs="?", default="./ptxc")
    parser.add_argument("--ptxas", help="optional path to NVIDIA ptxas; assemble every successful fixture")
    args = parser.parse_args()
    compiler_arg = args.compiler
    compiler = pathlib.Path(compiler_arg)
    if not compiler.is_absolute():
        compiler = (pathlib.Path.cwd() / compiler).resolve()
    if not compiler.exists():
        raise SystemExit(f"Compiler executable not found: {compiler}")
    with tempfile.TemporaryDirectory(prefix="ptxc-tests-") as tmp:
        out_dir = pathlib.Path(tmp)
        for index, relative in enumerate(SUCCESS):
            source = ROOT / relative
            output = out_dir / f"success_{index}.ptx"
            run = subprocess.run([str(compiler), str(source), "-o", str(output)], text=True, capture_output=True)
            if run.returncode != 0:
                raise SystemExit(f"Expected success for {relative}, got {run.returncode}:\n{run.stderr}")
            text = output.read_text(encoding="utf-8")
            if ".version 7.8" not in text or ".visible .entry" not in text or "ret;" not in text:
                raise SystemExit(f"Generated PTX for {relative} is missing expected structure")
            if relative.endswith("two_kernels.k") and text.count(".visible .entry") != 2:
                raise SystemExit("two_kernels.k should generate two PTX entry points")
            if args.ptxas:
                cubin = out_dir / f"success_{index}.cubin"
                assemble = subprocess.run([args.ptxas, "-arch=sm_75", str(output), "-o", str(cubin)], text=True, capture_output=True)
                if assemble.returncode != 0:
                    raise SystemExit(f"ptxas rejected generated PTX for {relative}:\n{assemble.stderr}")
        for relative, expected in ERRORS.items():
            output = out_dir / "should_not_exist.ptx"
            run = subprocess.run([str(compiler), str(ROOT / relative), "-o", str(output)], text=True, capture_output=True)
            if run.returncode == 0 or expected not in run.stderr:
                raise SystemExit(f"Expected {relative} to fail with '{expected}', got {run.returncode}:\n{run.stderr}")
            if not re.search(r"line \d+, column \d+:", run.stderr):
                raise SystemExit(f"Diagnostic for {relative} did not contain a line and column:\n{run.stderr}")
        ast_run = subprocess.run([str(compiler), str(ROOT / "examples/sum_to.k"), "-o", str(out_dir / "ast.ptx"), "--dump-ast"], text=True, capture_output=True)
        if ast_run.returncode != 0 or "Kernel sum_to" not in ast_run.stdout or "While" not in ast_run.stdout:
            raise SystemExit("--dump-ast did not print the expected kernel syntax tree")
    ptxas_text = f", and ptxas assembled all {len(SUCCESS)} outputs" if args.ptxas else ""
    print(f"CLI fixture tests passed: {len(SUCCESS)} valid programs, {len(ERRORS)} rejected programs, and AST dump{ptxas_text}.")


if __name__ == "__main__":
    main()
