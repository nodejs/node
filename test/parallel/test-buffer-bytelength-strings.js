// Flags: --allow-natives-syntax --expose_externalize_string
'use strict';

const common = require('../common');
const assert = require('assert');

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

const { byteLength } = Buffer;
const latin1 = Buffer.from('aé'.repeat(100), 'latin1').toString('latin1');
const external = createExternalizableString('external é'.repeat(20));
externalizeString(external);
assert.strictEqual(isOneByteString(external), true);

// Check flat, cons, sliced, two-byte and external strings, including lone
// surrogates, before and after optimizing the caller.
const cases = [
  'hello',
  'é',
  latin1,
  'x'.repeat(1000) + latin1,
  latin1.slice(1, 150),
  '€',
  '\ud83d',
  'a€😀'.repeat(200),
  external,
];

function test() {
  for (const string of cases) {
    assert.strictEqual(byteLength(string), Buffer.from(string).length);
  }
}

eval('%PrepareFunctionForOptimization(byteLength)');
test();
eval('%OptimizeFunctionOnNextCall(byteLength)');
test();
