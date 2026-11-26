// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-maglev-loop-peeling
// Disable Maglev loop peeling so `b1.v` in `test_immutable_load_replay` stays
// inside the loop header instead of being hoisted into a peeled preheader.

function sideeffect() {}
%NeverOptimizeFunction(sideeffect);

let other = { a: 1, b: 1 };
other.b = 2;

// 1. Tests promoting a mutable slot (from an initializing StoreOp) to immutable
// when dominated by an immutable LoadOp, so it survives `sideeffect()`.
function test_store_to_immutable_load(k, other) {
  let h = { a: 1, b: { v: k } };
  other.b = 1;
  let v1 = h.b.v;
  other.b = 2;
  let b2 = h.b;
  sideeffect();
  let v2 = b2.v;
  %TurbofanStaticAssert(v1 === k);
  %TurbofanStaticAssert(v2 === k);
}

%PrepareFunctionForOptimization(test_store_to_immutable_load);
test_store_to_immutable_load({}, other);
test_store_to_immutable_load({}, other);
%OptimizeFunctionOnNextCall(test_store_to_immutable_load);
test_store_to_immutable_load({}, other);

// 2. Tests that an immutable LoadOp key inserted in a loop header survives
// SnapshotTable rollback (RevertCurrentSnapshot) and replay (ReplaySnapshot)
// without being added to `base_keys_` and invalidated by `sideeffect()`.
function read_v(o) { return o.v; }
let mut = { v: {}, m: 1 };
mut.v = {};
%PrepareFunctionForOptimization(read_v);
read_v({ v: {} });
read_v(mut);

function test_immutable_load_replay(b, other) {
  let h = { a: 1, b };
  other.b = 1;
  let b1 = h.b;
  other.b = 2;
  let b2 = h.b;

  let v1;
  let i = 0;
  do {
    v1 = b1.v;
  } while (++i < 2);

  sideeffect();
  let v2 = read_v(b2);
  %TurbofanStaticAssert(v1 === v2);
}

%PrepareFunctionForOptimization(test_immutable_load_replay);
test_immutable_load_replay({ v: {} }, other);
test_immutable_load_replay({ v: {} }, other);
%OptimizeFunctionOnNextCall(test_immutable_load_replay);
test_immutable_load_replay({ v: {} }, other);
