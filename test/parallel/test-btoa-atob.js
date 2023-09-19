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
strictEqual(atob('  Y\fW\tJ\njZ A=\r= '), 'abcd');

strictEqual(atob(null), '\x9Eée');
strictEqual(atob(NaN), '5£');
strictEqual(atob(Infinity), '"wâ\x9E+r');
strictEqual(atob(true), '¶»\x9E');
strictEqual(atob(1234), '×mø');
strictEqual(atob([]), '');
strictEqual(atob({ toString: () => '' }), '');
strictEqual(atob({ [Symbol.toPrimitive]: () => '' }), '');

throws(() => atob(Symbol()), /TypeError/);
[
  undefined, false, () => {}, {}, [1],
  0, 1, 0n, 1n, -Infinity,
  'a', 'a\n\n\n', '\ra\r\r', '  a ', '\t\t\ta', 'a\f\f\f', '\ta\r \n\f',
].forEach((value) =>
  // See #2 - https://html.spec.whatwg.org/multipage/webappapis.html#dom-atob
  throws(() => atob(value), {
    constructor: DOMException,
    name: 'InvalidCharacterError',
    code: 5,
  }));
