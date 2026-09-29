// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Note that {o.x} must not be at offset 12, since this is also the length
// offset of arrays, and it will trip up Turbofan's load elimination, which will
// think that storing to {o.x} in the forEach could change the length of the
// array being iterated.
let o = { a : 25, x : 2, y : "abc" };
o.x = 43;
o.y = "x";
let arr = [1, 2, 3, 4];

function inner() {
  o.x = 25;
}

function outer() {
  // When reaching this header, {o.x} is Unobservable on the out-of-loop
  // successor and GCObservable on the in-loop successor => it's merged as
  // GCObservable.

  // Triggering a GC to make {o.x} GCObservable instead of Unobservable.
  o.y = { x : o.y };
  // Store to {o.x} to make it Unobservable.
  o.x = 17;

  // When reaching this loop header for the 1st time, {o.x} is Observable from
  // the outer-loop successor and Unobservable from the inner loop => it's
  // merged as Observable.
  // When reaching this loop header for the 2nd time, {o.x} is GCObersable
  // from the outer-loop successor and Unobservable from the inner loop =>
  // it's merged as GCObservable. This is the reason for this test:
  // StoreStoreElimination had a DCHECK that revisiting loop would only ever
  // widen observability, but it should in fact only narrow it.
  arr.forEach(inner);

  // When reaching this backedge for the first time, {o.x} is assumed to be
  // Observable.
  // When reaching it for the revisit, {o.x} is assumed to be GCObservable
  // (iff the StackCheck doesn't have CanRead effects! (which it does, but
  // for testing we can remove this effect)).
}

function foo() {
  // Inserting the CheckMaps for {o}. This has to happen before the loop because
  // CheckMaps lowers to a DeoptimizeIf, which has CanRead effects. The map is
  // stable, and nothing below can change it, so we won't need another CheckMaps
  // for {o} later.
  o.x = 111;

  arr.forEach(outer);

  // Store {o.x} to make it Unobservable above.
  o.x = 19;
}

%PrepareFunctionForOptimization(outer);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(foo);
foo([1, 2, 3, 4]);

%OptimizeFunctionOnNextCall(foo);
foo([1, 2, 3, 4]);
