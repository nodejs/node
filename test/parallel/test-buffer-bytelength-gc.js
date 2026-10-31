// Flags: --allow-natives-syntax --gc-global --gc-interval=500
'use strict';

require('../common');
const assert = require('assert');

function byteLength(object, string) {
  return object.x + Buffer.byteLength(string);
}

function makeDying() {
  const object = { x: 1 };
  object.dying = 1;
  return object;
}

const keep = { x: 2, keep: 1 };

// The optimized caller holds the dying objects' map weakly. A full GC while
// flattening a cons string can invalidate that map and lazily deoptimize it.
// A value-returning fast API call, including its slow fallback, cannot handle
// that deoptimization and aborts instead.
eval('%PrepareFunctionForOptimization(byteLength)');
for (let i = 0; i < 10; i++) {
  byteLength(makeDying(), 'ab');
  byteLength(keep, 'ab');
}
eval('%OptimizeFunctionOnNextCall(byteLength)');
assert.strictEqual(byteLength(keep, 'ab'), 4);

const big = 'a'.repeat(1 << 20);
for (let i = 0; i < 50; i++) {
  const cons = big + String(i) + big;
  assert.strictEqual(byteLength(keep, cons), 2 + cons.length);
}
