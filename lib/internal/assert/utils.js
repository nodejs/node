'use strict';

const {
  Error,
  ErrorCaptureStackTrace,
  ErrorPrototypeToString,
  StringPrototypeCharCodeAt,
  StringPrototypeReplace,
} = primordials;

const {
  codes: {
    ERR_AMBIGUOUS_ARGUMENT,
    ERR_INVALID_ARG_TYPE,
  },
  isErrorStackTraceLimitWritable,
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const { isError } = require('internal/util');
const { format } = require('internal/util/inspect');

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

/**
 * A function that derives the failure message from the actual and expected values.
 * It is invoked only when the assertion fails.
 *
 * Other return values than a string are ignored.
 * @callback MessageFactory
 * @param {any} actual
 * @param {any} expected
 * @returns {string}
 */

/**
 * Raw message input is always passed internally as a tuple array.
 * Accepted shapes:
 *  - []
 *  - [string]
 *  - [string, ...any[]] (printf-like substitutions)
 *  - [Error]
 *  - [MessageFactory]
 *
 * Additional elements after [Error] or [MessageFactory] are rejected with ERR_AMBIGUOUS_ARGUMENT.
 * A first element that is neither string, Error nor function is rejected with ERR_INVALID_ARG_TYPE.
 * @typedef {[] | [string] | [string, ...any[]] | [Error] | [MessageFactory]} MessageTuple
 */

/**
 * Options consumed by innerFail to construct and throw the AssertionError.
 * @typedef {object} InnerFailOptions
 * @property {any} actual Actual value
 * @property {any} expected Expected value
 * @property {MessageTuple} message Message
 * @property {string} operator Operator
 * @property {Function} stackStartFn Stack start function
 * @property {'simple' | 'full'} [diff] Diff mode
 * @property {boolean} [generatedMessage] Generated message
 */

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

/**
 * @param {InnerFailOptions} obj
 */
function innerFail(obj) {
  if (obj.message.length === 0) {
    obj.message = undefined;
  } else if (typeof obj.message[0] === 'string') {
    if (obj.message.length > 1) {
      obj.message = format(...obj.message);
    } else {
      obj.message = obj.message[0];
    }
  } else if (isError(obj.message[0])) {
    if (obj.message.length > 1) {
      throw new ERR_AMBIGUOUS_ARGUMENT(
        'message',
        `The error message was passed as error object "${ErrorPrototypeToString(obj.message[0])}" has trailing arguments that would be ignored.`,
      );
    }
    throw obj.message[0];
  } else if (typeof obj.message[0] === 'function') {
    if (obj.message.length > 1) {
      throw new ERR_AMBIGUOUS_ARGUMENT(
        'message',
        `The error message with function "${obj.message[0].name || 'anonymous'}" has trailing arguments that would be ignored.`,
      );
    }
    try {
      obj.message = obj.message[0](obj.actual, obj.expected);
      if (typeof obj.message !== 'string') {
        obj.message = undefined;
      }
    } catch {
      // Ignore and use default message instead
      obj.message = undefined;
    }
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      'message',
      ['string', 'function'],
      obj.message[0],
    );
  }

  const error = new AssertionError(obj);
  if (obj.generatedMessage !== undefined) {
    error.generatedMessage = obj.generatedMessage;
  }
  throw error;
}

/**
 * Internal ok handler delegating to innerFail for message handling.
 * @param {Function} fn
 * @param {...any} args
 */
function innerOk(fn, ...args) {
  if (!args[0]) {
    let generatedMessage = false;
    let messageArgs;

    if (args.length === 0) {
      generatedMessage = true;
      messageArgs = ['No value argument passed to `assert.ok()`'];
    } else if (args.length === 1 || args[1] == null) {
      generatedMessage = true;
      messageArgs = [getErrMessage(fn)];
    } else {
      messageArgs = args.slice(1);
    }

    innerFail({
      actual: args[0],
      expected: true,
      message: messageArgs,
      operator: '==',
      stackStartFn: fn,
      generatedMessage,
    });
  }
}

module.exports = {
  innerOk,
  innerFail,
};
