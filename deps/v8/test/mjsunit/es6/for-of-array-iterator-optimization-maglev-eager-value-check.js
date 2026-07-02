// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --for-of-optimization

// Map A: Reaches both 'done' and 'value' during warmup.
function MapAClass(v, d) {
  this.value = v;
  this.done = d;
}

// Map B: Different property insertion order creates a distinct Map.
// During warmup, this is only used to terminate the loop (done: true).
// Therefore, the 'value' property is NEVER read from Map B during warmup.
function MapBClass(d, v) {
  this.done = d;
  this.value = v;
}

// Pre-allocate the objects to guarantee their Maps never change.
// Use distinct values so the sum is unique (1, 10, 100).
const objA1 = new MapAClass(1, false);
const objA2 = new MapAClass(10, false);
const objB_term = new MapBClass(true, 0);

// The Trap: Uses the exact same Map B as the terminator,
// but sets done to false. It will pass the 'done' map check,
// and eagerly deopt on the 'value' map check!
const objB_trap = new MapBClass(false, 100);

let triggerDeopt = false;

const iterable = {
  [Symbol.iterator]() {
    let count = 0;
    const iterator = {
      next() {
        if (triggerDeopt) {
          // Yield the trap, and turn OFF the trigger so the
          // loop can finish gracefully on subsequent iterations!
          triggerDeopt = false;
          return objB_trap;
        }

        // Warmup: 2 normal iterations (Map A), then terminate (Map B).
        count++;
        if (count === 1) return objA1;
        if (count === 2) return objA2;
        return objB_term;
      }
    };
    %NeverOptimizeFunction(iterator.next);
    return iterator;
  }
};

function testForOfEagerDeoptValue() {
  let sum = 0;
  for (const x of iterable) {
    sum += x;
  }
  return sum;
}

// 1. Warm up
// 'done' IC sees MapAClass and MapBClass (Polymorphic).
// 'value' IC ONLY sees MapAClass (Monomorphic).
%PrepareFunctionForOptimization(testForOfEagerDeoptValue);
testForOfEagerDeoptValue();
testForOfEagerDeoptValue();

// 2. Optimize
%OptimizeMaglevOnNextCall(testForOfEagerDeoptValue);
testForOfEagerDeoptValue();

// 3. Trigger Eager Deopt specifically on the value load!
triggerDeopt = true;
const result = testForOfEagerDeoptValue();

assertUnoptimized(testForOfEagerDeoptValue);

// The sum calculation for the deopt run:
// 1. The objB_trap iteration (gracefully handled by your continuation!): 100
// 2. count=1 (objA1): 1
// 3. count=2 (objA2): 10
// 4. count=3 (objB_term): terminates.
// Total: 111!
assertEquals(111, result);
