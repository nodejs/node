// Test that functions can be wrapped multiple times and verify length and name
// properties are preserved correctly.

'use strict';

require('../common');
const assert = require('assert');
const { timerify } = require('perf_hooks');

const m = (a, b = 1) => {};
const n = timerify(m);
const o = timerify(m);
const p = timerify(n);
assert.notStrictEqual(n, o);
assert.notStrictEqual(n, p);
assert.notStrictEqual(o, p);
assert.strictEqual(n.length, m.length);
assert.strictEqual(n.name, 'timerified m');
assert.strictEqual(p.name, 'timerified timerified m');
