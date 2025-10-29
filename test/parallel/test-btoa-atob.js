'use strict';

require('../common');

const assert = require('assert');
const buffer = require('buffer');

// Exported on the global object
assert.strictEqual(globalThis.atob, buffer.atob);
assert.strictEqual(globalThis.btoa, buffer.btoa);

// Throws type error on no argument passed
assert.throws(() => buffer.atob(), /TypeError/);
assert.throws(() => buffer.btoa(), /TypeError/);

assert.strictEqual(atob(' '), '');
assert.strictEqual(atob('  Y\fW\tJ\njZ A=\r= '), 'abcd');

assert.strictEqual(atob(null), '\x9Eée');
assert.strictEqual(atob(NaN), '5£');
assert.strictEqual(atob(Infinity), '"wâ\x9E+r');
assert.strictEqual(atob(true), '¶»\x9E');
assert.strictEqual(atob(1234), '×mø');
assert.strictEqual(atob([]), '');
assert.strictEqual(atob({ toString: () => '' }), '');
assert.strictEqual(atob({ [Symbol.toPrimitive]: () => '' }), '');

assert.throws(() => atob(Symbol()), /TypeError/);
[
  undefined, false, () => {}, {}, [1],
  0, 1, 0n, 1n, -Infinity,
  'a', 'a\n\n\n', '\ra\r\r', '  a ', '\t\t\ta', 'a\f\f\f', '\ta\r \n\f',
].forEach((value) =>
  // See #2 - https://html.spec.whatwg.org/multipage/webappapis.html#dom-atob
  assert.throws(() => atob(value), {
    constructor: DOMException,
    name: 'InvalidCharacterError',
    code: 5,
  }));
