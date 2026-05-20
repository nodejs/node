// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function make_heapnum(v) { const a=[1.1]; a[0]=v; return a[0]; }

const H0 = 42;//make_heapnum(24440.0);
const H1 = make_heapnum(-1.0);

function foo(c, o) {
  let x = c ? H1 : H0;
  x + 2;
  if (c) {
    // Will have HeapObject field representation.
    o.x = x;
  } else {
    // Will have Smi field representation.
    o.y = x;
  }
}

let o_heapobj_field = { x : "abc" };
o_heapobj_field.x = []; // making non-const and removing field map.

let o_smi_field = { y : 17 };
o_smi_field.y = 25; // making non-const

%PrepareFunctionForOptimization(foo);
foo(true, o_heapobj_field);
foo(false, o_smi_field);

%OptimizeMaglevOnNextCall(foo);
foo(true, o_heapobj_field);
foo(false, o_smi_field);
