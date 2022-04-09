'use strict';

require('../common');

const { strictEqual, throws } = require('assert');
const buffer = require('buffer');

// Exported on the global object
strictEqual(globalThis.atob, buffer.atob);
strictEqual(globalThis.btoa, buffer.btoa);

// Throws type error on no argument passed
throws(() => buffer.atob(), /TypeError/);
throws(() => buffer.btoa(), /TypeError/);

strictEqual(atob(' '), '');
strictEqual(atob('  YW\tJ\njZA=\r= '), 'abcd');

strictEqual(atob(null), '\x9Eée');
strictEqual(atob(NaN), '5£');
strictEqual(atob(Infinity), '"wâ\x9E+r');
strictEqual(atob(true), '¶»\x9E');
strictEqual(atob(1234), '×mø');
strictEqual(atob([]), '');
strictEqual(atob({ toString: () => '' }), '');
strictEqual(atob({ [Symbol.toPrimitive]: () => '' }), '');

throws(() => atob(Symbol()), /TypeError/);
[undefined, false, () => {}, 0, 1, 0n, 1n, -Infinity, [1], {}].forEach((value) =>
  throws(() => atob(value), { constructor: DOMException }));
