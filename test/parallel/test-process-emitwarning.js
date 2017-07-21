// Flags: --no-warnings
// The flag suppresses stderr output but the warning event will still emit
'use strict';

const common = require('../common');
const assert = require('assert');

const testMsg = 'A Warning';
const testCode = 'CODE001';
const testDetail = 'Some detail';
const testType = 'CustomWarning';

process.on('warning', common.mustCall((warning) => {
  assert(warning);
  assert(/^(?:Warning|CustomWarning)/.test(warning.name));
  assert.strictEqual(warning.message, testMsg);
  if (warning.code) assert.strictEqual(warning.code, testCode);
  if (warning.detail) assert.strictEqual(warning.detail, testDetail);
}, 15));

class CustomWarning extends Error {
  constructor() {
    super();
    this.name = testType;
    this.message = testMsg;
    this.code = testCode;
    Error.captureStackTrace(this, CustomWarning);
  }
}

[
  [testMsg],
  [testMsg, testType],
  [testMsg, CustomWarning],
  [testMsg, testType, CustomWarning],
  [testMsg, testType, testCode],
  [testMsg, { type: testType }],
  [testMsg, { type: testType, code: testCode }],
  [testMsg, { type: testType, code: testCode, detail: testDetail }],
  [new CustomWarning()],
  // detail will be ignored for the following. No errors thrown
  [testMsg, { type: testType, code: testCode, detail: true }],
  [testMsg, { type: testType, code: testCode, detail: [] }],
  [testMsg, { type: testType, code: testCode, detail: null }],
  [testMsg, { type: testType, code: testCode, detail: 1 }]
].forEach((i) => {
  assert.doesNotThrow(() => process.emitWarning.apply(null, i));
});

const warningNoToString = new CustomWarning();
warningNoToString.toString = null;
process.emitWarning(warningNoToString);

const warningThrowToString = new CustomWarning();
warningThrowToString.toString = function() {
  throw new Error('invalid toString');
};
process.emitWarning(warningThrowToString);

const expectedError =
  common.expectsError({ code: 'ERR_INVALID_ARG_TYPE', type: TypeError }, 11);

// TypeError is thrown on invalid input
assert.throws(() => process.emitWarning(1), expectedError);
assert.throws(() => process.emitWarning({}), expectedError);
assert.throws(() => process.emitWarning(true), expectedError);
assert.throws(() => process.emitWarning([]), expectedError);
assert.throws(() => process.emitWarning('', '', {}), expectedError);
assert.throws(() => process.emitWarning('', 1), expectedError);
assert.throws(() => process.emitWarning('', '', 1), expectedError);
assert.throws(() => process.emitWarning('', true), expectedError);
assert.throws(() => process.emitWarning('', '', true), expectedError);
assert.throws(() => process.emitWarning('', []), expectedError);
assert.throws(() => process.emitWarning('', '', []), expectedError);
