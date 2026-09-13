// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function get_obj() {
  // Useless stuff to increase bytecode size so that `get_obj` gets inlined
  // after `update`.
  let v = 1;
  v+v+v+v+v+v+v+v+v+v+v+v+v;

  return { e : {} }
}

const unused = { a: 4.2 };

function update(o) {
  const local_obj = {
    a: 42
  };
  local_obj.a = 42;
  o.a = NaN;
}

function test() {
  const obj = get_obj();
  update(obj);
}

%PrepareFunctionForOptimization(get_obj);
%PrepareFunctionForOptimization(update);
%PrepareFunctionForOptimization(test);
test();
test();

%OptimizeFunctionOnNextCall(test);
test();
