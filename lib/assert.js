// Originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the 'Software'), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const { Buffer } = require('buffer');
const {
  isDeepEqual,
  isDeepStrictEqual
} = require('internal/util/comparisons');
const {
  AssertionError,
  errorCache,
  codes: {
    ERR_INVALID_ARG_TYPE
  }
} = require('internal/errors');
const { openSync, closeSync, readSync } = require('fs');
const { inspect } = require('util');
const { EOL } = require('os');
const { NativeModule } = require('internal/bootstrap/loaders');

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
  '\\u001e', '\\u001f'
];

const escapeFn = (str) => meta[str.charCodeAt(0)];

const ERR_DIFF_DEACTIVATED = 0;
const ERR_DIFF_NOT_EQUAL = 1;
const ERR_DIFF_EQUAL = 2;

let warned = false;

// The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.

const assert = module.exports = ok;

const NO_EXCEPTION_SENTINEL = {};

// All of the following functions must throw an AssertionError
// when a corresponding condition is not met, with a message that
// may be undefined if not provided. All assertion methods provide
// both the actual and expected values to the assertion error for
// display purposes.

function innerFail(obj) {
  if (obj.message instanceof Error) throw obj.message;

  throw new AssertionError(obj);
}

function fail(actual, expected, message, operator, stackStartFn) {
  const argsLen = arguments.length;

  if (argsLen === 0) {
    message = 'Failed';
  } else if (argsLen === 1) {
    message = actual;
    actual = undefined;
  } else {
    if (warned === false) {
      warned = true;
      process.emitWarning(
        'assert.fail() with more than one argument is deprecated. ' +
          'Please use assert.strictEqual() instead or only pass a message.',
        'DeprecationWarning',
        'DEP0094'
      );
    }
    if (argsLen === 2)
      operator = '!=';
  }

  innerFail({
    actual,
    expected,
    message,
    operator,
    stackStartFn: stackStartFn || fail
  });
}

assert.fail = fail;

// The AssertionError is defined in internal/error.
// new assert.AssertionError({ message: message,
//                             actual: actual,
//                             expected: expected });
assert.AssertionError = AssertionError;

function getBuffer(fd, assertLine) {
  var lines = 0;
  // Prevent blocking the event loop by limiting the maximum amount of
  // data that may be read.
  var maxReads = 64; // bytesPerRead * maxReads = 512 kb
  var bytesRead = 0;
  var startBuffer = 0; // Start reading from that char on
  const bytesPerRead = 8192;
  const buffers = [];
  do {
    const buffer = Buffer.allocUnsafe(bytesPerRead);
    bytesRead = readSync(fd, buffer, 0, bytesPerRead);
    for (var i = 0; i < bytesRead; i++) {
      if (buffer[i] === 10) {
        lines++;
        if (lines === assertLine) {
          startBuffer = i + 1;
        // Read up to 15 more lines to make sure all code gets matched
        } else if (lines === assertLine + 16) {
          buffers.push(buffer.slice(startBuffer, i));
          return buffers;
        }
      }
    }
    if (lines >= assertLine) {
      buffers.push(buffer.slice(startBuffer, bytesRead));
      // Reset the startBuffer in case we need more than one chunk
      startBuffer = 0;
    }
  } while (--maxReads !== 0 && bytesRead !== 0);
  return buffers;
}

function getErrMessage(call) {
  const filename = call.getFileName();
  const line = call.getLineNumber() - 1;
  const column = call.getColumnNumber() - 1;
  const identifier = `${filename}${line}${column}`;

  if (errorCache.has(identifier)) {
    return errorCache.get(identifier);
  }

  // Skip Node.js modules!
  if (filename.endsWith('.js') && NativeModule.exists(filename.slice(0, -3))) {
    errorCache.set(identifier, undefined);
    return;
  }

  var fd;
  try {
    fd = openSync(filename, 'r', 0o666);
    const buffers = getBuffer(fd, line);
    const code = Buffer.concat(buffers).toString('utf8');
    // Lazy load acorn.
    const { parseExpressionAt } = require('internal/deps/acorn/dist/acorn');
    const nodes = parseExpressionAt(code, column);
    // Node type should be "CallExpression" and some times
    // "SequenceExpression".
    const node = nodes.type === 'CallExpression' ? nodes : nodes.expressions[0];
    const name = node.callee.name;
    // Calling `ok` with .apply or .call is uncommon but we use a simple
    // safeguard nevertheless.
    if (name !== 'apply' && name !== 'call') {
    // Only use `assert` and `assert.ok` to reference the "real API" and
    // not user defined function names.
      const ok = name === 'ok' ? '.ok' : '';
      const args = node.arguments;
      var message = code
        .slice(args[0].start, args[args.length - 1].end)
        .replace(escapeSequencesRegExp, escapeFn);
      message = 'The expression evaluated to a falsy value:' +
        `${EOL}${EOL}  assert${ok}(${message})${EOL}`;
    }
    // Make sure to always set the cache! No matter if the message is
    // undefined or not
    errorCache.set(identifier, message);

    return message;

  } catch (e) {
  // Invalidate cache to prevent trying to read this part again.
    errorCache.set(identifier, undefined);
  } finally {
    if (fd !== undefined)
      closeSync(fd);
  }
}

function innerOk(fn, argLen, value, message) {
  if (!value) {
    let generatedMessage = false;

    if (argLen === 0) {
      generatedMessage = true;
      message = 'No value argument passed to `assert.ok()`';
    } else if (message == null) {
      // Use the call as error message if possible.
      // This does not work with e.g. the repl.
      // eslint-disable-next-line no-restricted-syntax
      const err = new Error();
      // Make sure the limit is set to 1. Otherwise it could fail (<= 0) or it
      // does to much work.
      const tmpLimit = Error.stackTraceLimit;
      Error.stackTraceLimit = 1;
      Error.captureStackTrace(err, fn);
      Error.stackTraceLimit = tmpLimit;

      const tmpPrepare = Error.prepareStackTrace;
      Error.prepareStackTrace = (_, stack) => stack;
      const call = err.stack[0];
      Error.prepareStackTrace = tmpPrepare;

      // Make sure it would be "null" in case that is used.
      message = getErrMessage(call) || message;
      generatedMessage = true;
    } else if (message instanceof Error) {
      throw message;
    }

    const err = new AssertionError({
      actual: value,
      expected: true,
      message,
      operator: '==',
      stackStartFn: fn
    });
    err.generatedMessage = generatedMessage;
    throw err;
  }
}

// Pure assertion tests whether a value is truthy, as determined
// by !!value.
function ok(...args) {
  innerOk(ok, args.length, ...args);
}
assert.ok = ok;

// The equality assertion tests shallow, coercive equality with ==.
/* eslint-disable no-restricted-properties */
assert.equal = function equal(actual, expected, message) {
  // eslint-disable-next-line eqeqeq
  if (actual != expected) {
    innerFail({
      actual,
      expected,
      message,
      operator: '==',
      stackStartFn: equal
    });
  }
};

// The non-equality assertion tests for whether two objects are not
// equal with !=.
assert.notEqual = function notEqual(actual, expected, message) {
  // eslint-disable-next-line eqeqeq
  if (actual == expected) {
    innerFail({
      actual,
      expected,
      message,
      operator: '!=',
      stackStartFn: notEqual
    });
  }
};

// The equivalence assertion tests a deep equality relation.
assert.deepEqual = function deepEqual(actual, expected, message) {
  if (!isDeepEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'deepEqual',
      stackStartFn: deepEqual
    });
  }
};

// The non-equivalence assertion tests for any deep inequality.
assert.notDeepEqual = function notDeepEqual(actual, expected, message) {
  if (isDeepEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'notDeepEqual',
      stackStartFn: notDeepEqual
    });
  }
};
/* eslint-enable */

assert.deepStrictEqual = function deepStrictEqual(actual, expected, message) {
  if (!isDeepStrictEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'deepStrictEqual',
      stackStartFn: deepStrictEqual,
      errorDiff: this === strict ? ERR_DIFF_EQUAL : ERR_DIFF_DEACTIVATED
    });
  }
};

assert.notDeepStrictEqual = notDeepStrictEqual;
function notDeepStrictEqual(actual, expected, message) {
  if (isDeepStrictEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'notDeepStrictEqual',
      stackStartFn: notDeepStrictEqual,
      errorDiff: this === strict ? ERR_DIFF_NOT_EQUAL : ERR_DIFF_DEACTIVATED
    });
  }
}

assert.strictEqual = function strictEqual(actual, expected, message) {
  if (!Object.is(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'strictEqual',
      stackStartFn: strictEqual,
      errorDiff: this === strict ? ERR_DIFF_EQUAL : ERR_DIFF_DEACTIVATED
    });
  }
};

assert.notStrictEqual = function notStrictEqual(actual, expected, message) {
  if (Object.is(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'notStrictEqual',
      stackStartFn: notStrictEqual,
      errorDiff: this === strict ? ERR_DIFF_NOT_EQUAL : ERR_DIFF_DEACTIVATED
    });
  }
};

class Comparison {
  constructor(obj, keys) {
    for (const key of keys) {
      if (key in obj)
        this[key] = obj[key];
    }
  }
}

function compareExceptionKey(actual, expected, key, message, keys) {
  if (!(key in actual) || !isDeepStrictEqual(actual[key], expected[key])) {
    if (!message) {
      // Create placeholder objects to create a nice output.
      const a = new Comparison(actual, keys);
      const b = new Comparison(expected, keys);

      const tmpLimit = Error.stackTraceLimit;
      Error.stackTraceLimit = 0;
      const err = new AssertionError({
        actual: a,
        expected: b,
        operator: 'deepStrictEqual',
        stackStartFn: assert.throws,
        errorDiff: ERR_DIFF_EQUAL
      });
      Error.stackTraceLimit = tmpLimit;
      message = err.message;
    }
    innerFail({
      actual,
      expected,
      message,
      operator: 'throws',
      stackStartFn: assert.throws
    });
  }
}

function expectedException(actual, expected, msg) {
  if (typeof expected !== 'function') {
    if (expected instanceof RegExp)
      return expected.test(actual);
    // assert.doesNotThrow does not accept objects.
    if (arguments.length === 2) {
      throw new ERR_INVALID_ARG_TYPE(
        'expected', ['Function', 'RegExp'], expected
      );
    }
    const keys = Object.keys(expected);
    // Special handle errors to make sure the name and the message are compared
    // as well.
    if (expected instanceof Error) {
      keys.push('name', 'message');
    }
    for (const key of keys) {
      compareExceptionKey(actual, expected, key, msg, keys);
    }
    return true;
  }
  // Guard instanceof against arrow functions as they don't have a prototype.
  if (expected.prototype !== undefined && actual instanceof expected) {
    return true;
  }
  if (Error.isPrototypeOf(expected)) {
    return false;
  }
  return expected.call({}, actual) === true;
}

function getActual(block) {
  if (typeof block !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('block', 'Function', block);
  }
  try {
    block();
  } catch (e) {
    return e;
  }
  return NO_EXCEPTION_SENTINEL;
}

async function waitForActual(block) {
  if (typeof block !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('block', 'Function', block);
  }

  // Return a rejected promise if `block` throws synchronously.
  const resultPromise = block();
  try {
    await resultPromise;
  } catch (e) {
    return e;
  }
  return NO_EXCEPTION_SENTINEL;
}

function expectsError(stackStartFn, actual, error, message) {
  if (typeof error === 'string') {
    if (arguments.length === 4) {
      throw new ERR_INVALID_ARG_TYPE('error',
                                     ['Object', 'Error', 'Function', 'RegExp'],
                                     error);
    }
    message = error;
    error = null;
  }

  if (actual === NO_EXCEPTION_SENTINEL) {
    let details = '';
    if (error && error.name) {
      details += ` (${error.name})`;
    }
    details += message ? `: ${message}` : '.';
    const fnType = stackStartFn === rejects ? 'rejection' : 'exception';
    innerFail({
      actual,
      expected: error,
      operator: stackStartFn.name,
      message: `Missing expected ${fnType}${details}`,
      stackStartFn
    });
  }
  if (error && expectedException(actual, error, message) === false) {
    throw actual;
  }
}

function expectsNoError(stackStartFn, actual, error, message) {
  if (actual === NO_EXCEPTION_SENTINEL)
    return;

  if (typeof error === 'string') {
    message = error;
    error = null;
  }

  if (!error || expectedException(actual, error)) {
    const details = message ? `: ${message}` : '.';
    const fnType = stackStartFn === doesNotReject ? 'rejection' : 'exception';
    innerFail({
      actual,
      expected: error,
      operator: stackStartFn.name,
      message: `Got unwanted ${fnType}${details}\n${actual && actual.message}`,
      stackStartFn
    });
  }
  throw actual;
}

function throws(block, ...args) {
  expectsError(throws, getActual(block), ...args);
}

assert.throws = throws;

async function rejects(block, ...args) {
  expectsError(rejects, await waitForActual(block), ...args);
}

assert.rejects = rejects;

function doesNotThrow(block, ...args) {
  expectsNoError(doesNotThrow, getActual(block), ...args);
}

assert.doesNotThrow = doesNotThrow;

async function doesNotReject(block, ...args) {
  expectsNoError(doesNotReject, await waitForActual(block), ...args);
}

assert.doesNotReject = doesNotReject;

assert.ifError = function ifError(err) {
  if (err !== null && err !== undefined) {
    let message = 'ifError got unwanted exception: ';
    if (typeof err === 'object' && typeof err.message === 'string') {
      if (err.message.length === 0 && err.constructor) {
        message += err.constructor.name;
      } else {
        message += err.message;
      }
    } else {
      message += inspect(err);
    }

    const newErr = new AssertionError({
      actual: err,
      expected: null,
      operator: 'ifError',
      message,
      stackStartFn: ifError
    });

    // Make sure we actually have a stack trace!
    const origStack = err.stack;

    if (typeof origStack === 'string') {
      // This will remove any duplicated frames from the error frames taken
      // from within `ifError` and add the original error frames to the newly
      // created ones.
      const tmp2 = origStack.split('\n');
      tmp2.shift();
      // Filter all frames existing in err.stack.
      let tmp1 = newErr.stack.split('\n');
      for (var i = 0; i < tmp2.length; i++) {
        // Find the first occurrence of the frame.
        const pos = tmp1.indexOf(tmp2[i]);
        if (pos !== -1) {
          // Only keep new frames.
          tmp1 = tmp1.slice(0, pos);
          break;
        }
      }
      newErr.stack = `${tmp1.join('\n')}\n${tmp2.join('\n')}`;
    }

    throw newErr;
  }
};

// Expose a strict only variant of assert
function strict(...args) {
  innerOk(strict, args.length, ...args);
}
assert.strict = Object.assign(strict, assert, {
  equal: assert.strictEqual,
  deepEqual: assert.deepStrictEqual,
  notEqual: assert.notStrictEqual,
  notDeepEqual: assert.notDeepStrictEqual
});
assert.strict.strict = assert.strict;
