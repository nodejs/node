// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noturbo-inlining --noturbo-verify-allocation

// Ensure that very large stack frames can be used successfully.
// The flag --noturbo-verify-allocation is to make this run a little faster; it
// shouldn't affect the behavior.

const frame_size = 4096 * 4;       // 4 pages
const num_locals = frame_size / 8; // Assume 8-byte floating point values

function f() { return 0.1; }

// Function g, on positive inputs, will call itself recursively. On negative
// inputs, it does a computation that requires a large number of locals.
// The flag --noturbo-inlining is important to keep the compiler from realizing
// that all of this work is for nothing.
let g_text = "if (input === 0) return; if (input > 0) return g(input - 1);";
g_text += " var inc = f(); var a0 = 0;";
for (let i = 1; i < num_locals; ++i) {
  g_text += " var a" + i + " = a" + (i - 1) + " + inc;";
}
g_text += " return f(a0";
for (let i = 1; i < num_locals; ++i) {
  g_text += ", a" + i;
}
g_text += ");";
const g = new Function("input", g_text);

%PrepareFunctionForOptimization(g);
g(1);
g(-1);
%OptimizeFunctionOnNextCall(g);

// Use recursion to get past whatever stack space is already committed.
// 20 * 16kB = 320kB, comfortably below the default 1MB stack reservation limit.
g(20);
