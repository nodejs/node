// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax --expose-gc

function f(a) {
  let phi = a ? 0 : 4.2; // Phi untagging will untag this to a Float64
  phi |= 0; // Causing a CheckedSmiUntag to be inserted
  a.c = phi; // The graph builder will insert a StoreTaggedFieldNoWriteBarrier
             // because `phi` is a Smi. Afterphi untagging, this should become a
             // StoreTaggedFieldWithWriteBarrier, because `phi` is now a float.
}

// Allocating an object and making it old (its `c` field should be neither a Smi
// nor a Double, so that the graph builder inserts a StoreTaggedFieldxxx rather
// than a StoreDoubleField or CheckedStoreSmiField).
let obj = {c:"a"};
gc();
gc();

%PrepareFunctionForOptimization(f);
f(obj);

%OptimizeMaglevOnNextCall(f);
// This call to `f` will store a young object into that `c` field of `obj`. This
// should be done with a write barrier.
f(obj);

// If the write barrier was dropped, the GC will complain because it will see an
// old->new pointer without remembered set entry.
gc();
