'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');

const {
  canCopyArrayBuffer,
  cloneAsUint8Array,
  getArrayBufferView,
  privateSymbols: {
    arrow_message_private_symbol,
  },
} = internalBinding('util');

const obj = {};
assert.strictEqual(obj[arrow_message_private_symbol], undefined);

obj[arrow_message_private_symbol] = 'bar';
assert.strictEqual(obj[arrow_message_private_symbol], 'bar');
assert.deepStrictEqual(Reflect.ownKeys(obj), []);

let arrowMessage;

try {
  require(fixtures.path('syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage = err[arrow_message_private_symbol];
}

assert.match(arrowMessage, /bad_syntax\.js:1/);

{
  const view = new Uint8Array(new ArrayBuffer(8), 2, 4);
  assert.deepStrictEqual(getArrayBufferView(view), [view.buffer, 2, 4]);

  const sabView = new Uint8Array(new SharedArrayBuffer(8), 2, 4);
  assert.deepStrictEqual(getArrayBufferView(sabView), [sabView.buffer, 2, 4]);
}

{
  const source = new Uint8Array([1, 2, 3, 4]);
  const clone = cloneAsUint8Array(source.subarray(1, 3));
  assert.deepStrictEqual([...clone], [2, 3]);
  assert.notStrictEqual(clone.buffer, source.buffer);
}

{
  const to = new ArrayBuffer(8);
  const from = new ArrayBuffer(8);
  const sab = new SharedArrayBuffer(8);
  assert.strictEqual(canCopyArrayBuffer(to, 0, from, 0, 8), true);
  assert.strictEqual(canCopyArrayBuffer(sab, 0, from, 0, 8), true);
  assert.strictEqual(canCopyArrayBuffer(to, 2 ** 32, from, 0, 1), false);
  assert.strictEqual(canCopyArrayBuffer(to, 0, from, 0, 2 ** 32), false);
}
