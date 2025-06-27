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
  assert.match(warning.name, /^(?:Warning|CustomWarning)/);
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
  // Detail will be ignored for the following. No errors thrown
  [testMsg, { type: testType, code: testCode, detail: true }],
  [testMsg, { type: testType, code: testCode, detail: [] }],
  [testMsg, { type: testType, code: testCode, detail: null }],
  [testMsg, { type: testType, code: testCode, detail: 1 }],
].forEach((args) => {
  process.emitWarning(...args);
});

const warningNoToString = new CustomWarning();
warningNoToString.toString = null;
process.emitWarning(warningNoToString);

const warningThrowToString = new CustomWarning();
warningThrowToString.toString = function() {
  throw new Error('invalid toString');
};
process.emitWarning(warningThrowToString);

// TypeError is thrown on invalid input
[
  [1],
  [{}],
  [true],
  [[]],
  ['', '', {}],
  ['', 1],
  ['', '', 1],
  ['', true],
  ['', '', true],
  ['', []],
  ['', '', []],
  [],
  [undefined, 'foo', 'bar'],
  [undefined],
].forEach((args) => {
  assert.throws(
    () => process.emitWarning(...args),
    { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' }
  );
});
