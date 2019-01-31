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
const { codes: {
  ERR_AMBIGUOUS_ARGUMENT,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_RETURN_VALUE
} } = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const { openSync, closeSync, readSync } = require('fs');
const { inspect, types: { isPromise, isRegExp } } = require('util');
const { EOL } = require('internal/constants');
const { NativeModule } = require('internal/bootstrap/loaders');

const errorCache = new Map();

let isDeepEqual;
let isDeepStrictEqual;
let parseExpressionAt;
let findNodeAround;
let columnOffset = 0;
let decoder;

function lazyLoadComparison() {
  const comparison = require('internal/util/comparisons');
  isDeepEqual = comparison.isDeepEqual;
  isDeepStrictEqual = comparison.isDeepStrictEqual;
}

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

  let internalMessage;
  if (argsLen === 0) {
    internalMessage = 'Failed';
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

  if (message instanceof Error) throw message;

  const errArgs = {
    actual,
    expected,
    operator: operator === undefined ? 'fail' : operator,
    stackStartFn: stackStartFn || fail
  };
  if (message !== undefined) {
    errArgs.message = message;
  }
  const err = new AssertionError(errArgs);
  if (internalMessage) {
    err.message = internalMessage;
    err.generatedMessage = true;
  }
  throw err;
}

assert.fail = fail;

// The AssertionError is defined in internal/error.
assert.AssertionError = AssertionError;

function findColumn(fd, column, code) {
  if (code.length > column + 100) {
    try {
      return parseCode(code, column);
    } catch {
      // End recursion in case no code could be parsed. The expression should
      // have been found after 2500 characters, so stop trying.
      if (code.length - column > 2500) {
        // eslint-disable-next-line no-throw-literal
        throw null;
      }
    }
  }
  // Read up to 2500 bytes more than necessary in columns. That way we address
  // multi byte characters and read enough data to parse the code.
  const bytesToRead = column - code.length + 2500;
  const buffer = Buffer.allocUnsafe(bytesToRead);
  const bytesRead = readSync(fd, buffer, 0, bytesToRead);
  code += decoder.write(buffer.slice(0, bytesRead));
  // EOF: fast path.
  if (bytesRead < bytesToRead) {
    return parseCode(code, column);
  }
  // Read potentially missing code.
  return findColumn(fd, column, code);
}

function getCode(fd, line, column) {
  let bytesRead = 0;
  if (line === 0) {
    // Special handle line number one. This is more efficient and simplifies the
    // rest of the algorithm. Read more than the regular column number in bytes
    // to prevent multiple reads in case multi byte characters are used.
    return findColumn(fd, column, '');
  }
  let lines = 0;
  // Prevent blocking the event loop by limiting the maximum amount of
  // data that may be read.
  let maxReads = 64; // bytesPerRead * maxReads = 512 kb
  const bytesPerRead = 8192;
  // Use a single buffer up front that is reused until the call site is found.
  let buffer = Buffer.allocUnsafe(bytesPerRead);
  while (maxReads-- !== 0) {
    // Only allocate a new buffer in case the needed line is found. All data
    // before that can be discarded.
    buffer = lines < line ? buffer : Buffer.allocUnsafe(bytesPerRead);
    bytesRead = readSync(fd, buffer, 0, bytesPerRead);
    // Read the buffer until the required code line is found.
    for (var i = 0; i < bytesRead; i++) {
      if (buffer[i] === 10 && ++lines === line) {
        // If the end of file is reached, directly parse the code and return.
        if (bytesRead < bytesPerRead) {
          return parseCode(buffer.toString('utf8', i + 1, bytesRead), column);
        }
        // Check if the read code is sufficient or read more until the whole
        // expression is read. Make sure multi byte characters are preserved
        // properly by using the decoder.
        const code = decoder.write(buffer.slice(i + 1, bytesRead));
        return findColumn(fd, column, code);
      }
    }
  }
}

function parseCode(code, offset) {
  // Lazy load acorn.
  if (parseExpressionAt === undefined) {
    ({ parseExpressionAt } = require('internal/deps/acorn/acorn/dist/acorn'));
    ({ findNodeAround } = require('internal/deps/acorn/acorn-walk/dist/walk'));
  }
  let node;
  let start = 0;
  // Parse the read code until the correct expression is found.
  do {
    try {
      node = parseExpressionAt(code, start);
      start = node.end + 1 || start;
      // Find the CallExpression in the tree.
      node = findNodeAround(node, offset, 'CallExpression');
    } catch (err) {
      // Unexpected token error and the like.
      start += err.raisedAt || 1;
      if (start > offset) {
        // No matching expression found. This could happen if the assert
        // expression is bigger than the provided buffer.
        // eslint-disable-next-line no-throw-literal
        throw null;
      }
    }
  } while (node === undefined || node.node.end < offset);

  return [
    node.node.start,
    code.slice(node.node.start, node.node.end)
        .replace(escapeSequencesRegExp, escapeFn)
  ];
}

function getErrMessage(message, fn) {
  const tmpLimit = Error.stackTraceLimit;
  // Make sure the limit is set to 1. Otherwise it could fail (<= 0) or it
  // does to much work.
  Error.stackTraceLimit = 1;
  // We only need the stack trace. To minimize the overhead use an object
  // instead of an error.
  const err = {};
  Error.captureStackTrace(err, fn);
  Error.stackTraceLimit = tmpLimit;

  const tmpPrepare = Error.prepareStackTrace;
  Error.prepareStackTrace = (_, stack) => stack;
  const call = err.stack[0];
  Error.prepareStackTrace = tmpPrepare;

  const filename = call.getFileName();

  if (!filename) {
    return message;
  }

  const line = call.getLineNumber() - 1;
  let column = call.getColumnNumber() - 1;

  // Line number one reports the wrong column due to being wrapped in a
  // function. Remove that offset to get the actual call.
  if (line === 0) {
    if (columnOffset === 0) {
      const { wrapper } = require('internal/modules/cjs/loader');
      columnOffset = wrapper[0].length;
    }
    column -= columnOffset;
  }

  const identifier = `${filename}${line}${column}`;

  if (errorCache.has(identifier)) {
    return errorCache.get(identifier);
  }

  // Skip Node.js modules!
  if (filename.endsWith('.js') && NativeModule.exists(filename.slice(0, -3))) {
    errorCache.set(identifier, undefined);
    return;
  }

  let fd;
  try {
    // Set the stack trace limit to zero. This makes sure unexpected token
    // errors are handled faster.
    Error.stackTraceLimit = 0;

    if (decoder === undefined) {
      const { StringDecoder } = require('string_decoder');
      decoder = new StringDecoder('utf8');
    }

    fd = openSync(filename, 'r', 0o666);
    // Reset column and message.
    [column, message] = getCode(fd, line, column);
    // Flush unfinished multi byte characters.
    decoder.end();
    // Always normalize indentation, otherwise the message could look weird.
    if (message.indexOf('\n') !== -1) {
      if (EOL === '\r\n') {
        message = message.replace(/\r\n/g, '\n');
      }
      const frames = message.split('\n');
      message = frames.shift();
      for (const frame of frames) {
        let pos = 0;
        while (pos < column && (frame[pos] === ' ' || frame[pos] === '\t')) {
          pos++;
        }
        message += `\n  ${frame.slice(pos)}`;
      }
    }
    message = `The expression evaluated to a falsy value:\n\n  ${message}\n`;
    // Make sure to always set the cache! No matter if the message is
    // undefined or not
    errorCache.set(identifier, message);

    return message;
  } catch {
    // Invalidate cache to prevent trying to read this part again.
    errorCache.set(identifier, undefined);
  } finally {
    // Reset limit.
    Error.stackTraceLimit = tmpLimit;
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
      generatedMessage = true;
      message = getErrMessage(message, fn);
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
  if (isDeepEqual === undefined) lazyLoadComparison();
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
  if (isDeepEqual === undefined) lazyLoadComparison();
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
  if (isDeepEqual === undefined) lazyLoadComparison();
  if (!isDeepStrictEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'deepStrictEqual',
      stackStartFn: deepStrictEqual
    });
  }
};

assert.notDeepStrictEqual = notDeepStrictEqual;
function notDeepStrictEqual(actual, expected, message) {
  if (isDeepEqual === undefined) lazyLoadComparison();
  if (isDeepStrictEqual(actual, expected)) {
    innerFail({
      actual,
      expected,
      message,
      operator: 'notDeepStrictEqual',
      stackStartFn: notDeepStrictEqual
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
      stackStartFn: strictEqual
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
      stackStartFn: notStrictEqual
    });
  }
};

class Comparison {
  constructor(obj, keys, actual) {
    for (const key of keys) {
      if (key in obj) {
        if (actual !== undefined &&
            typeof actual[key] === 'string' &&
            isRegExp(obj[key]) &&
            obj[key].test(actual[key])) {
          this[key] = actual[key];
        } else {
          this[key] = obj[key];
        }
      }
    }
  }
}

function compareExceptionKey(actual, expected, key, message, keys) {
  if (!(key in actual) || !isDeepStrictEqual(actual[key], expected[key])) {
    if (!message) {
      // Create placeholder objects to create a nice output.
      const a = new Comparison(actual, keys);
      const b = new Comparison(expected, keys, actual);

      const err = new AssertionError({
        actual: a,
        expected: b,
        operator: 'deepStrictEqual',
        stackStartFn: assert.throws
      });
      err.actual = actual;
      err.expected = expected;
      err.operator = 'throws';
      throw err;
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
    if (isRegExp(expected))
      return expected.test(actual);
    // assert.doesNotThrow does not accept objects.
    if (arguments.length === 2) {
      throw new ERR_INVALID_ARG_TYPE(
        'expected', ['Function', 'RegExp'], expected
      );
    }

    // Handle primitives properly.
    if (typeof actual !== 'object' || actual === null) {
      const err = new AssertionError({
        actual,
        expected,
        message: msg,
        operator: 'deepStrictEqual',
        stackStartFn: assert.throws
      });
      err.operator = 'throws';
      throw err;
    }

    const keys = Object.keys(expected);
    // Special handle errors to make sure the name and the message are compared
    // as well.
    if (expected instanceof Error) {
      keys.push('name', 'message');
    } else if (keys.length === 0) {
      throw new ERR_INVALID_ARG_VALUE('error',
                                      expected, 'may not be an empty object');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    for (const key of keys) {
      if (typeof actual[key] === 'string' &&
          isRegExp(expected[key]) &&
          expected[key].test(actual[key])) {
        continue;
      }
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

function getActual(fn) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('fn', 'Function', fn);
  }
  try {
    fn();
  } catch (e) {
    return e;
  }
  return NO_EXCEPTION_SENTINEL;
}

function checkIsPromise(obj) {
  // Accept native ES6 promises and promises that are implemented in a similar
  // way. Do not accept thenables that use a function as `obj` and that have no
  // `catch` handler.
  return isPromise(obj) ||
    obj !== null && typeof obj === 'object' &&
    typeof obj.then === 'function' &&
    typeof obj.catch === 'function';
}

async function waitForActual(promiseFn) {
  let resultPromise;
  if (typeof promiseFn === 'function') {
    // Return a rejected promise if `promiseFn` throws synchronously.
    resultPromise = promiseFn();
    // Fail in case no promise is returned.
    if (!checkIsPromise(resultPromise)) {
      throw new ERR_INVALID_RETURN_VALUE('instance of Promise',
                                         'promiseFn', resultPromise);
    }
  } else if (checkIsPromise(promiseFn)) {
    resultPromise = promiseFn;
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      'promiseFn', ['Function', 'Promise'], promiseFn);
  }

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
    if (typeof actual === 'object' && actual !== null) {
      if (actual.message === error) {
        throw new ERR_AMBIGUOUS_ARGUMENT(
          'error/message',
          `The error message "${actual.message}" is identical to the message.`
        );
      }
    } else if (actual === error) {
      throw new ERR_AMBIGUOUS_ARGUMENT(
        'error/message',
        `The error "${actual}" is identical to the message.`
      );
    }
    message = error;
    error = undefined;
  } else if (error != null &&
             typeof error !== 'object' &&
             typeof error !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('error',
                                   ['Object', 'Error', 'Function', 'RegExp'],
                                   error);
  }

  if (actual === NO_EXCEPTION_SENTINEL) {
    let details = '';
    if (error && error.name) {
      details += ` (${error.name})`;
    }
    details += message ? `: ${message}` : '.';
    const fnType = stackStartFn.name === 'rejects' ? 'rejection' : 'exception';
    innerFail({
      actual: undefined,
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
    error = undefined;
  }

  if (!error || expectedException(actual, error)) {
    const details = message ? `: ${message}` : '.';
    const fnType = stackStartFn.name === 'doesNotReject' ?
      'rejection' : 'exception';
    innerFail({
      actual,
      expected: error,
      operator: stackStartFn.name,
      message: `Got unwanted ${fnType}${details}\n` +
               `Actual message: "${actual && actual.message}"`,
      stackStartFn
    });
  }
  throw actual;
}

assert.throws = function throws(promiseFn, ...args) {
  expectsError(throws, getActual(promiseFn), ...args);
};

assert.rejects = async function rejects(promiseFn, ...args) {
  expectsError(rejects, await waitForActual(promiseFn), ...args);
};

assert.doesNotThrow = function doesNotThrow(fn, ...args) {
  expectsNoError(doesNotThrow, getActual(fn), ...args);
};

assert.doesNotReject = async function doesNotReject(fn, ...args) {
  expectsNoError(doesNotReject, await waitForActual(fn), ...args);
};

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
