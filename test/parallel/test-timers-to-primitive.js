'use strict';
const common = require('../common');
const assert = require('assert');

const timeout1 = setTimeout(common.mustNotCall(), 1);
const timeout2 = setInterval(common.mustNotCall(), 1);

assert.strictEqual(Number.isNaN(+timeout1), false);
assert.strictEqual(Number.isNaN(+timeout2), false);

assert.strictEqual(+timeout1, timeout1[Symbol.toPrimitive]());

assert.notStrictEqual(`${timeout1}`, Object.prototype.toString.call(timeout1));
assert.notStrictEqual(`${timeout2}`, Object.prototype.toString.call(timeout2));

assert.notStrictEqual(+timeout1, +timeout2);

const o = {};
o[timeout1] = timeout1;
o[timeout2] = timeout2;
const keys = Object.keys(o);
assert.deepStrictEqual(keys, [`${timeout1}`, `${timeout2}`]);

clearTimeout(keys[0]); // Works for string.
clearInterval(+timeout2); // Works for integer.
