// Flags: --expose-internals --no-warnings --allow-natives-syntax --expose_externalize_string
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { byteLengthUtf8Fast } = internalBinding('buffer');

const {
  createExternalizableString,
  createExternalizableTwoByteString,
  externalizeString,
  isOneByteString,
} = globalThis;
common.allowGlobals(
  createExternalizableString,
  createExternalizableTwoByteString,
  externalizeString,
  isOneByteString,
  globalThis.x,
);

const flat = ['', 'hello', '\xe9', '\x00\x01\x7f\x80\xff'];
for (const length of [1, 7, 8, 9, 127, 128, 129, 1024]) {
  flat.push(Buffer.alloc(length, 0xe9).toString('latin1'));
}
const latin1 = flat[flat.length - 1];
const externalOneByte = createExternalizableString('external é'.repeat(20));
const externalTwoByte = createExternalizableTwoByteString('external €'.repeat(20));
externalizeString(externalOneByte);
externalizeString(externalTwoByte);
const fallback = [
  'x'.repeat(1000) + latin1,
  latin1.slice(1, 1000),
  '€',
  '\ud83d',
  'a€😀'.repeat(200),
  externalOneByte,
  externalTwoByte,
];

function fastLength(string) {
  return byteLengthUtf8Fast(string);
}

eval('%PrepareFunctionForOptimization(fastLength)');
for (const string of [...flat, ...fallback]) {
  // Before optimization the callback only returns the rejection sentinel.
  assert.strictEqual(fastLength(string), -1);
}
eval('%OptimizeFunctionOnNextCall(fastLength)');
for (const string of flat) {
  assert.strictEqual(fastLength(string), Buffer.from(string).length);
}
for (const string of fallback) {
  assert.strictEqual(fastLength(string), -1);
}
assert.strictEqual(fastLength('hello'), 5);

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(
    getV8FastApiCallCount('Buffer::FastByteLengthUtf8'), flat.length + 1);
}

const { byteLength } = Buffer;
function test() {
  for (const string of [...flat, ...fallback]) {
    assert.strictEqual(byteLength(string), Buffer.from(string).length);
    assert.strictEqual(byteLength(string, 'UTF-8'), Buffer.from(string).length);
    assert.strictEqual(byteLength(string, 'unknown'), Buffer.from(string).length);
  }
}
eval('%PrepareFunctionForOptimization(byteLength)');
test();
eval('%OptimizeFunctionOnNextCall(byteLength)');
test();
