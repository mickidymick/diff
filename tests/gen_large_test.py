#!/usr/bin/env python3
# Generates two realistic C-like test files for diff benchmarking.
# File A and B are ~15000 lines each with ~12% of lines changed,
# clustered in patches (like real code edits) and scattered throughout.

import random

random.seed(1337)

RET_TYPES   = ["int", "void", "char *", "float", "double", "unsigned int", "long", "size_t"]
PARAM_TYPES = ["int", "char *", "float", "void *", "unsigned int", "size_t", "long", "const char *"]
PARAM_NAMES = ["a", "b", "c", "x", "y", "z", "n", "len", "buf", "ptr", "val", "idx", "src", "dst"]
ARITH_OPS   = ["+", "-", "*", "/", "%", "&", "|", "^"]

def rvar(i):
    return f"var_{random.randint(0, max(0, i - 1))}" if i > 0 else "0"

def gen_function(fidx):
    lines = []
    ret   = random.choice(RET_TYPES)
    n_p   = random.randint(0, 4)
    params = [f"{random.choice(PARAM_TYPES)} {PARAM_NAMES[i]}" for i in range(n_p)]
    lines.append(f"{ret} func_{fidx}({', '.join(params)}) {{")

    n_body = random.randint(8, 35)
    for i in range(n_body):
        r = random.random()
        if r < 0.12:
            lines.append(f"    if ({rvar(i)} > {random.randint(0, 100)}) {{")
            lines.append(f"        {rvar(i)} = {rvar(i)} {random.choice(ARITH_OPS)} {random.randint(1, 10)};")
            lines.append(f"    }}")
        elif r < 0.20:
            lines.append(f"    for (int i_{fidx}_{i} = 0; i_{fidx}_{i} < {random.randint(2, 50)}; i_{fidx}_{i}++) {{")
            lines.append(f"        {rvar(i)} {random.choice(ARITH_OPS)}= i_{fidx}_{i};")
            lines.append(f"    }}")
        elif r < 0.28:
            lines.append(f"    switch ({rvar(i)} % {random.randint(2, 6)}) {{")
            for case in range(random.randint(2, 4)):
                lines.append(f"        case {case}:")
                lines.append(f"            {rvar(i)} = {random.randint(0, 999)};")
                lines.append(f"            break;")
            lines.append(f"        default: break;")
            lines.append(f"    }}")
        elif r < 0.38:
            lines.append(f"    int var_{i} = {rvar(i)} {random.choice(ARITH_OPS)} {random.randint(1, 100)};")
        elif r < 0.48:
            lines.append(f"    /* step {i}: process {rvar(i)} for func_{fidx} */")
        elif r < 0.55:
            lines.append(f"    printf(\"%d\\n\", {rvar(i)});")
        elif r < 0.62:
            lines.append(f"    memset(buf_{fidx}_{i}, 0, sizeof(buf_{fidx}_{i}));")
        elif r < 0.68:
            lines.append(f"    if (!{rvar(i)}) return {'0' if ret == 'int' else ''};")
        else:
            lines.append(f"    {rvar(i)} = func_{random.randint(max(0, fidx - 5), fidx)}();")

    if ret != "void":
        lines.append(f"    return {rvar(n_body)};")
    lines.append(f"}}")
    lines.append(f"")
    return lines


def gen_file(target_lines):
    out = []
    out += ["#include <stdio.h>", "#include <stdlib.h>", "#include <string.h>",
            "#include <assert.h>", "#include <stdint.h>", ""]
    fidx = 0
    while len(out) < target_lines:
        out.extend(gen_function(fidx))
        fidx += 1
    return out[:target_lines], fidx


# -- Build file A -----------------------------------------------------------
file_a, n_funcs = gen_file(15000)

# -- Build file B: apply clustered patches to file_a -----------------------
# Generate patch locations: ~10 clusters of contiguous changes + scattered singles.
file_b = list(file_a)

def apply_patches(lines, seed):
    random.seed(seed)
    result = list(lines)
    n      = len(result)
    edited = set()

    # Clustered patches (simulate refactoring regions)
    n_clusters = 18
    for _ in range(n_clusters):
        start  = random.randint(0, n - 1)
        length = random.randint(10, 80)
        for offset in range(length):
            pos = start + offset
            if pos >= len(result) or pos in edited:
                continue
            edited.add(pos)
            action = random.randint(0, 2)
            if action == 0:   # modify
                result[pos] = f"    int patched_{pos} = {random.randint(0, 9999)};  /* modified */"
            elif action == 1: # delete — mark for removal
                result[pos] = None
            else:             # insert after
                result.insert(pos + 1, f"    /* inserted at {pos} */ int ins_{pos} = 0;")

    # Scattered single-line changes
    n_singles = 400
    positions = random.sample(range(len(result)), min(n_singles, len(result)))
    for pos in positions:
        if pos in edited or result[pos] is None:
            continue
        edited.add(pos)
        if random.random() < 0.5:
            result[pos] = f"    /* changed */ int chg_{pos} = {random.randint(0, 9999)};"
        else:
            result.insert(pos, f"    int scattered_{pos} = 0; /* scatter insert */")

    return [l for l in result if l is not None]

file_b = apply_patches(file_a, seed=9999)

# Trim/pad so both are ~15000 lines
file_a = file_a[:15000]
file_b = file_b[:15000]

with open("large_a.c", "w") as f:
    f.write("\n".join(file_a) + "\n")

with open("large_b.c", "w") as f:
    f.write("\n".join(file_b) + "\n")

a_len = len(file_a)
b_len = len(file_b)
print(f"large_a.c: {a_len} lines")
print(f"large_b.c: {b_len} lines")
