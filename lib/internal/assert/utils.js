'use strict';

const {
  Error,
  ErrorCaptureStackTrace,
  StringPrototypeCharCodeAt,
  StringPrototypeReplace,
} = primordials;

const {
  isErrorStackTraceLimitWritable,
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const { isError } = require('internal/util');

const {
  getErrorSourceExpression,
} = require('internal/errors/error_source');

// Escape control characters but not \n and \t to keep the line breaks and
// indentation intact.
// eslint-disable-next-line no-control-regex
const escapeSequencesRegExp = /[\x00-\x08\x0b\x0c\x0e-\x1f]/g;
const meta = [
  '\\u0000', '\\u0001', '\\u0002', '\\u0003', '\\u0004',
  '\\u0005', '\\u0006', '\\u0007', '\\b', '',
  '', '\\u000b', '\\f', '', '\\u000e',
  '\\u000f', '\\u0010', '\\u0011', '\\u0012', '\\u0013',
  '\\u0014', '\\u0015', '\\u0016', '\\u0017', '\\u0018',
  '\\u0019', '\\u001a', '\\u001b', '\\u001c', '\\u001d',
  '\\u001e', '\\u001f',
];

const escapeFn = (str) => meta[StringPrototypeCharCodeAt(str, 0)];

function getErrMessage(fn) {
  const tmpLimit = Error.stackTraceLimit;
  const errorStackTraceLimitIsWritable = isErrorStackTraceLimitWritable();
  // Make sure the limit is set to 1. Otherwise it could fail (<= 0) or it
  // does to much work.
  if (errorStackTraceLimitIsWritable) Error.stackTraceLimit = 1;
  // We only need the stack trace. To minimize the overhead use an object
  // instead of an error.
  const err = {};
  ErrorCaptureStackTrace(err, fn);
  if (errorStackTraceLimitIsWritable) Error.stackTraceLimit = tmpLimit;

  let source = getErrorSourceExpression(err);
  if (source) {
    source = StringPrototypeReplace(source, escapeSequencesRegExp, escapeFn);
    return `The expression evaluated to a falsy value:\n\n  ${source}\n`;
  }
}

function innerOk(fn, argLen, value, message) {
  if (!value) {
    let generatedMessage = false;

    if (argLen === 0) {
      generatedMessage = true;
      message = 'No value argument passed to `assert.ok()`';
    } else if (message == null) {
      generatedMessage = true;
      message = getErrMessage(fn);
    } else if (isError(message)) {
      throw message;
    }

    const err = new AssertionError({
      actual: value,
      expected: true,
      message,
      operator: '==',
      stackStartFn: fn,
    });
    err.generatedMessage = generatedMessage;
    throw err;
  }
}

module.exports = {
  innerOk,
};
