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
const { AssertionError, TypeError } = require('internal/errors');
const { openSync, closeSync, readSync } = require('fs');
const { parseExpressionAt } = require('internal/deps/acorn/dist/acorn');
const { inspect } = require('util');
const { EOL } = require('os');

const codeCache = new Map();
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
  } else if (argsLen === 2) {
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

function innerOk(args, fn) {
  var [value, message] = args;

  if (args.length === 0)
    throw new TypeError('ERR_MISSING_ARGS', 'value');

  if (!value) {
    if (message == null) {
      // Use the call as error message if possible.
      // This does not work with e.g. the repl.
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

      const filename = call.getFileName();
      const line = call.getLineNumber() - 1;
      const column = call.getColumnNumber() - 1;
      const identifier = `${filename}${line}${column}`;

      if (codeCache.has(identifier)) {
        message = codeCache.get(identifier);
      } else {
        var fd;
        try {
          fd = openSync(filename, 'r', 0o666);
          const buffers = getBuffer(fd, line);
          const code = Buffer.concat(buffers).toString('utf8');
          const nodes = parseExpressionAt(code, column);
          // Node type should be "CallExpression" and some times
          // "SequenceExpression".
          const node = nodes.type === 'CallExpression' ?
            nodes :
            nodes.expressions[0];
          // TODO: fix the "generatedMessage property"
          // Since this is actually a generated message, it has to be
          // determined differently from now on.

          const name = node.callee.name;
          // Calling `ok` with .apply or .call is uncommon but we use a simple
          // safeguard nevertheless.
          if (name !== 'apply' && name !== 'call') {
            // Only use `assert` and `assert.ok` to reference the "real API" and
            // not user defined function names.
            const ok = name === 'ok' ? '.ok' : '';
            const args = node.arguments;
            message = code
              .slice(args[0].start, args[args.length - 1].end)
              .replace(escapeSequencesRegExp, escapeFn);
            message = 'The expression evaluated to a falsy value:' +
              `${EOL}${EOL}  assert${ok}(${message})${EOL}`;
          }
          // Make sure to always set the cache! No matter if the message is
          // undefined or not
          codeCache.set(identifier, message);
        } catch (e) {
          // Invalidate cache to prevent trying to read this part again.
          codeCache.set(identifier, undefined);
        } finally {
          if (fd !== undefined)
            closeSync(fd);
        }
      }
    }
    innerFail({
      actual: value,
      expected: true,
      message,
      operator: '==',
      stackStartFn: fn
    });
  }
}

// Pure assertion tests whether a value is truthy, as determined
// by !!value.
function ok(...args) {
  innerOk(args, ok);
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

function createMsg(msg, key, actual, expected) {
  if (msg)
    return msg;
  return `${key}: expected ${inspect(expected[key])}, ` +
    `not ${inspect(actual[key])}`;
}

function expectedException(actual, expected, msg) {
  if (typeof expected !== 'function') {
    if (expected instanceof RegExp)
      return expected.test(actual);
    // assert.doesNotThrow does not accept objects.
    if (arguments.length === 2) {
      throw new TypeError('ERR_INVALID_ARG_TYPE', 'expected',
                          ['Function', 'RegExp'], expected);
    }
    // The name and message could be non enumerable. Therefore test them
    // explicitly.
    if ('name' in expected) {
      assert.strictEqual(
        actual.name,
        expected.name,
        createMsg(msg, 'name', actual, expected));
    }
    if ('message' in expected) {
      assert.strictEqual(
        actual.message,
        expected.message,
        createMsg(msg, 'message', actual, expected));
    }
    const keys = Object.keys(expected);
    for (const key of keys) {
      assert.deepStrictEqual(
        actual[key],
        expected[key],
        createMsg(msg, key, actual, expected));
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
    throw new TypeError('ERR_INVALID_ARG_TYPE', 'block', 'Function',
                        block);
  }
  try {
    block();
  } catch (e) {
    return e;
  }
  return NO_EXCEPTION_SENTINEL;
}

// Expected to throw an error.
assert.throws = function throws(block, error, message) {
  const actual = getActual(block);

  if (typeof error === 'string') {
    if (arguments.length === 3)
      throw new TypeError('ERR_INVALID_ARG_TYPE',
                          'error',
                          ['Function', 'RegExp'],
                          error);

    message = error;
    error = null;
  }

  if (actual === NO_EXCEPTION_SENTINEL) {
    let details = '';
    if (error && error.name) {
      details += ` (${error.name})`;
    }
    details += message ? `: ${message}` : '.';
    innerFail({
      actual,
      expected: error,
      operator: 'throws',
      message: `Missing expected exception${details}`,
      stackStartFn: throws
    });
  }
  if (error && expectedException(actual, error, message) === false) {
    throw actual;
  }
};

assert.doesNotThrow = function doesNotThrow(block, error, message) {
  const actual = getActual(block);
  if (actual === NO_EXCEPTION_SENTINEL)
    return;

  if (typeof error === 'string') {
    message = error;
    error = null;
  }

  if (!error || expectedException(actual, error)) {
    const details = message ? `: ${message}` : '.';
    innerFail({
      actual,
      expected: error,
      operator: 'doesNotThrow',
      message: `Got unwanted exception${details}\n${actual && actual.message}`,
      stackStartFn: doesNotThrow
    });
  }
  throw actual;
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
  innerOk(args, strict);
}
assert.strict = Object.assign(strict, assert, {
  equal: assert.strictEqual,
  deepEqual: assert.deepStrictEqual,
  notEqual: assert.notStrictEqual,
  notDeepEqual: assert.notDeepStrictEqual
});
assert.strict.strict = assert.strict;
