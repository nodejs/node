// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const ta = new Int32Array();
const hn = %AllocateHeapNumber();
const smi_obj = { y: 1 };
function get_42() { return 42; }
const target_str = "hello_" + get_42();

function bar(s, run) {
  if (run) return s === target_str;
}
%PrepareFunctionForOptimization(bar);
bar("other", true);

function foo(cond, y, run) {
  let x = cond ? hn : undefined;
  ta[x];
  let c = (y & 1) < 2;
  let a = x ^ (c ? 0 : 1);
  smi_obj.y = a;
  return bar(c ? a : "str", run);
}

%PrepareFunctionForOptimization(foo);
foo(true, 0, false);

%OptimizeFunctionOnNextCall(foo);
assertEquals(false, foo(true, 0, true));
