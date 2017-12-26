// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { TextDecoder, TextEncoder } = require('util');
const { customInspectSymbol: inspect } = require('internal/util');

// This test checks TextEncoder's instantiation, default encoding,
// encoding capabilities, and conformity with util.inspect().

{
  const textEncoder = new TextEncoder();
  assert.ok(textEncoder);
  assert.strictEqual(textEncoder instanceof TextEncoder, true);
}

{
  const textEncoder = new TextEncoder();
  assert.strictEqual(textEncoder.encoding, 'utf-8');
}

function checkIfBuffersAreEqual(buffer1, buffer2) {
  const comparisonResult = Buffer.compare(buffer1, buffer2);
  return comparisonResult === 0;
}

{
  const EURO_CURRENCY_SIGN = '\u20AC';
  const testString = 'test' + EURO_CURRENCY_SIGN;

  const textEncoder = new TextEncoder();
  const bufferForEncodedTestString = textEncoder.encode(testString);
  const expectedBuffer = Buffer.from(testString, 'utf8');

  const encodingWasSuccessful = checkIfBuffersAreEqual(
    bufferForEncodedTestString,
    expectedBuffer
  );

  assert.strictEqual(encodingWasSuccessful, true);
}

{
  const textEncoder = new TextEncoder();
  const bufferForEmptyEncoding = textEncoder.encode();
  assert.strictEqual(bufferForEmptyEncoding.length, 0);
}

{
  const textEncoder = new TextEncoder();
  const bufferForUndefinedEncoding = textEncoder.encode(undefined);
  assert.strictEqual(bufferForUndefinedEncoding.length, 0);
}

const textEncoderInspectFunc = TextEncoder.prototype[inspect];
const recursiveInspectDepth = Infinity;
const inspectOptions = {};

assert.doesNotThrow(() => {
  textEncoderInspectFunc.call(
    new TextEncoder(),
    recursiveInspectDepth,
    inspectOptions
  );
});

const contextsForThis = [
  {},
  [],
  true,
  1,
  '',
  new TextDecoder()
];

function testInspectFuncWithContextForThis(context) {
  const invalidThisContextError = {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type TextEncoder'
  };

  common.expectsError(
    () => textEncoderInspectFunc.call(
      context,
      recursiveInspectDepth,
      inspectOptions
    ),
    invalidThisContextError
  );
}

contextsForThis.forEach(testInspectFuncWithContextForThis);
