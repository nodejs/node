// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function foo(b) {
  if (b) {
    return 42;
  } else {
    return { x : b };
  }
}
%PrepareFunctionForOptimization(foo);
foo(true);
foo(false);
foo(false);

function bar(b, skip) {
  if (skip) return;
  let old_obj = { x : 42 };
  let x = foo(b);
  x | 0;
  old_obj.x = x;
  return old_obj;
}
%PrepareFunctionForOptimization(bar);
bar(true, false);
bar(true, false);
%PretenureAllocationSite(bar(true, false));
gc({type: 'minor'});
bar(true, false);

function get_false(arg) {
  // Increasing bytecode size so that it gets inlined last.
  arg+arg+arg+arg+arg+arg+arg+arg+arg+arg;
  arg+arg+arg+arg+arg+arg+arg+arg+arg+arg;
  arg+arg+arg+arg+arg+arg+arg+arg+arg+arg;
  return false;
}
%PrepareFunctionForOptimization(get_false);
get_false();

function baz(skip) {
  let b = get_false();
  bar(b, skip);
}
%PrepareFunctionForOptimization(baz);
baz(true);

%OptimizeFunctionOnNextCall(baz);
baz(false);
