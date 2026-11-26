// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining
// Flags: --min-maglev-inlining-frequency=0

// Make the initial JSArrayIterator map unstable.
([].keys()).x = 1;

// Its map has a Smi field at the same offset as
// JSArrayIterator::iterated_object_.
let g_obj = {a: 1};

function getReceiver() {}
%PrepareFunctionForOptimization(getReceiver);
%DisableEagerInlining(getReceiver);

function sideEffect() {}
%NeverOptimizeFunction(sideEffect);

class Base {}
class Derived extends Base {
  constructor(o) {
    // When `o` is the inlined iterator allocation, this load is folded to
    // `iterated_object_` (undefined) and gives the undefined constant an empty
    // type.
    o.a;
    // Initializing the in-object property `p` of the inlined allocation with
    // undefined aborts.
    super();
    this.p = 1;
  }
}
%PrepareFunctionForOptimization(Derived);
%DisableEagerInlining(Derived);

let run_foo = true;
function foo(mode) {
  if (!run_foo) return;
  let r = getReceiver();
  if (mode) r = g_obj;
  g_obj = r;
  let iter = Array.prototype.keys.call(r);
  sideEffect();
  new Derived(mode ? r : iter);
}
%PrepareFunctionForOptimization(foo);

function main() {
  return foo(0);
}
%PrepareFunctionForOptimization(main);

foo(1);
run_foo = false;
main();
%OptimizeMaglevOnNextCall(main);
main();
assertMaglevved(main);
