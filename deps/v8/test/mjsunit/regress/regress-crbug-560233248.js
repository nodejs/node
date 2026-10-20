// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbolev

// Replace and transition a global cell's value between the compiler's value
// and map reads, leaving the compiler with the replaced object's map.

const template = [1.125, 1.125, 1.125];
const marker = {};

// The named property keeps these arrays off the shared array maps, so the
// object-elements map below stays stable. slice() copies without an allocation
// site, so transitioning a copy does not generalize later copies.
function mkDouble() { const a = template.slice(); a.tag = 0; return a; }
function transition(a) { a[1] = marker; return a; }

// Bring the cell to kConstantType and warm the store IC's property cell case.
// From here on, storing a same-map value does not change the cell details.
gl = mkDouble();
function set(x) { gl = x; }
for (let i = 0; i < 100; i++) set(mkDouble());

const objElements = transition(mkDouble());

const kPoolSize = 500;
function mkPool() {
  const pool = [];
  for (let i = 0; i < kPoolSize; i++) pool.push(mkDouble());
  return pool;
}
// Rotate the cell through same-map arrays, transitioning each one as soon as
// it leaves the cell.
function rotate(pool) {
  let prev = mkDouble();
  for (let i = 0; i < kPoolSize; i++) {
    const t = pool[i];
    set(t);
    transition(prev);
    prev = t;
  }
  set(mkDouble());
}

for (let j = 0; j < 10; j++) {
  // Each attempt needs fresh functions: the element access must only ever see
  // the object-elements map, and one read through gl would make it
  // polymorphic. The interpolated j keeps the compilation cache from reusing
  // the script.
  const [get, read] = Function(`
      function get(a, real) {
        if (real) return a[0];
        return ${j};
      }
      return [get, function read(real) { return get(gl, real); }];`)();
  %PrepareFunctionForOptimization(get);
  %PrepareFunctionForOptimization(read);
  for (let i = 0; i < 5; i++) {
    get(objElements, true);
    get(objElements, false);
    read(false);
  }

  // Only stores made during the compile can hit the window between the
  // broker's two reads, so fill the pool first and start rotating as soon as
  // the job is dispatched.
  const pool = mkPool();
  %OptimizeFunctionOnNextCall(read, "concurrent");
  read(false);
  rotate(pool);
  %WaitForBackgroundOptimization();
  %FinalizeOptimization();
  // gl holds a packed-double array, so this must produce its double element.
  // The stale map made this load read that double as a tagged pointer.
  assertEquals(1.125, read(true));
}
