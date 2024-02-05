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

const invalidChar = {
  name: 'InvalidCharacterError'
};

// Test the entire 16 bit space for invalid characters.
for (let i = 0; i <= 0xffff; i++) {
  switch (i) {
    case 0x09: // \t
    case 0x0A: // \n
    case 0x0C: // \f
    case 0x0D: // \r
    case 0x20: // ' '
    case 0x2B: // +
    case 0x2F: // /
    case 0x3D: // =
      continue;
  }

  // 0-9
  if (i >= 0x30 && i <= 0x39)
    continue;

  // A-Z
  if (i >= 0x41 && i <= 0x5a)
    continue;

  // a-z
  if (i >= 0x61 && i <= 0x7a)
    continue;

  const ch = String.fromCharCode(i);

  throws(() => atob(ch), invalidChar);
  throws(() => atob('a' + ch), invalidChar);
  throws(() => atob('aa' + ch), invalidChar);
  throws(() => atob('aaa' + ch), invalidChar);
  throws(() => atob(ch + 'a'), invalidChar);
  throws(() => atob(ch + 'aa'), invalidChar);
  throws(() => atob(ch + 'aaa'), invalidChar);
}

throws(() => btoa('abcd\ufeffx'), invalidChar);

const charset =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function randomString(size) {
  let str = '';

  for (let i = 0; i < size; i++)
    str += charset[Math.random() * charset.length | 0];

  while (str.length & 3)
    str += '=';

  return str;
}

for (let i = 0; i < 100; i++) {
  const str = randomString(200);
  strictEqual(btoa(atob(str)), str);
}

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
