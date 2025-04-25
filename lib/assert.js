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

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Error,
  NumberIsNaN,
  ObjectAssign,
  ObjectIs,
  ObjectKeys,
  ObjectPrototypeIsPrototypeOf,
  ReflectApply,
  RegExpPrototypeExec,
  String,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
} = primordials;

const {
  codes: {
    ERR_AMBIGUOUS_ARGUMENT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_RETURN_VALUE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const { inspect } = require('internal/util/inspect');
const {
  isPromise,
  isRegExp,
} = require('internal/util/types');
const { isError } = require('internal/util');
const { innerOk } = require('internal/assert/utils');

const {
  validateFunction,
} = require('internal/validators');

let isDeepEqual;
let isDeepStrictEqual;
let isPartialStrictEqual;

function lazyLoadComparison() {
  const comparison = require('internal/util/comparisons');
  isDeepEqual = comparison.isDeepEqual;
  isDeepStrictEqual = comparison.isDeepStrictEqual;
  isPartialStrictEqual = comparison.isPartialStrictEqual;
}

// The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.
const assert = module.exports = ok;

const NO_EXCEPTION_SENTINEL = {};

class Comparison {
  constructor(obj, keys, actual) {
    for (const key of keys) {
      if (key in obj) {
        if (actual !== undefined &&
            typeof actual[key] === 'string' &&
            isRegExp(obj[key]) &&
            RegExpPrototypeExec(obj[key], actual[key]) !== null) {
          this[key] = actual[key];
        } else {
          this[key] = obj[key];
        }
      }
    }
  }
}

/**
 * Assert options.
 * @typedef {object} AssertOptions
 * @property {'full'|'simple'} [diff='simple'] - If set to 'full', shows the full diff in assertion errors.
 */

/**
 * The Assert class provides assertion methods.
 * @class
 * @param {AssertOptions} [options] - Optional configuration for assertions.
 */
class Assert {
  constructor(options = {}) {
    this.AssertionError = AssertionError;
    this.options = { ...options, diff: options.diff ?? 'simple' };
  }

  #buildAssertionErrorOptions(obj) {
    if (this.options.diff === 'full') {
      return { ...obj, diff: this.options.diff };
    }
    return obj;
  }

  // All of the following functions must throw an AssertionError
  // when a corresponding condition is not met, with a message that
  // may be undefined if not provided. All assertion methods provide
  // both the actual and expected values to the assertion error for
  // display purposes.

  #innerFail(obj) {
    if (obj.message instanceof Error) throw obj.message;

    throw new AssertionError(this.#buildAssertionErrorOptions(obj));
  }

  #internalMatch(string, regexp, message, fn) {
    if (!isRegExp(regexp)) {
      throw new ERR_INVALID_ARG_TYPE(
        'regexp', 'RegExp', regexp,
      );
    }
    const match = fn === Assert.prototype.match;
    if (typeof string !== 'string' ||
        RegExpPrototypeExec(regexp, string) !== null !== match) {
      if (message instanceof Error) {
        throw message;
      }

      const generatedMessage = !message;

      // 'The input was expected to not match the regular expression ' +
      message ||= (typeof string !== 'string' ?
        'The "string" argument must be of type string. Received type ' +
          `${typeof string} (${inspect(string)})` :
        (match ?
          'The input did not match the regular expression ' :
          'The input was expected to not match the regular expression ') +
            `${inspect(regexp)}. Input:\n\n${inspect(string)}\n`);
      const err = new AssertionError(this.#buildAssertionErrorOptions({
        actual: string,
        expected: regexp,
        message,
        operator: fn.name,
        stackStartFn: fn,
      }));
      err.generatedMessage = generatedMessage;
      throw err;
    }
  }

  #compareExceptionKey(actual, expected, key, message, keys, fn) {
    if (!(key in actual) || !isDeepStrictEqual(actual[key], expected[key])) {
      if (!message) {
        // Create placeholder objects to create a nice output.
        const a = new Comparison(actual, keys);
        const b = new Comparison(expected, keys, actual);

        const err = new AssertionError(this.#buildAssertionErrorOptions({
          actual: a,
          expected: b,
          operator: 'deepStrictEqual',
          stackStartFn: fn,
        }));
        err.actual = actual;
        err.expected = expected;
        err.operator = fn.name;
        throw err;
      }
      this.#innerFail({
        actual,
        expected,
        message,
        operator: fn.name,
        stackStartFn: fn,
      });
    }
  }

  #expectedException(actual, expected, message, fn) {
    let generatedMessage = false;
    let throwError = false;

    if (typeof expected !== 'function') {
      // Handle regular expressions.
      if (isRegExp(expected)) {
        const str = String(actual);
        if (RegExpPrototypeExec(expected, str) !== null)
          return;

        if (!message) {
          generatedMessage = true;
          message = 'The input did not match the regular expression ' +
                    `${inspect(expected)}. Input:\n\n${inspect(str)}\n`;
        }
        throwError = true;
        // Handle primitives properly.
      } else if (typeof actual !== 'object' || actual === null) {
        const err = new AssertionError(this.#buildAssertionErrorOptions({
          actual,
          expected,
          message,
          operator: 'deepStrictEqual',
          stackStartFn: fn,
        }));
        err.operator = fn.name;
        throw err;
      } else {
        // Handle validation objects.
        const keys = ObjectKeys(expected);
        // Special handle errors to make sure the name and the message are
        // compared as well.
        if (expected instanceof Error) {
          ArrayPrototypePush(keys, 'name', 'message');
        } else if (keys.length === 0) {
          throw new ERR_INVALID_ARG_VALUE('error',
                                          expected, 'may not be an empty object');
        }
        if (isDeepEqual === undefined) lazyLoadComparison();
        for (const key of keys) {
          if (typeof actual[key] === 'string' &&
              isRegExp(expected[key]) &&
              RegExpPrototypeExec(expected[key], actual[key]) !== null) {
            continue;
          }
          this.#compareExceptionKey(actual, expected, key, message, keys, fn);
        }
        return;
      }
    // Guard instanceof against arrow functions as they don't have a prototype.
    // Check for matching Error classes.
    } else if (expected.prototype !== undefined && actual instanceof expected) {
      return;
    } else if (ObjectPrototypeIsPrototypeOf(Error, expected)) {
      if (!message) {
        generatedMessage = true;
        message = 'The error is expected to be an instance of ' +
          `"${expected.name}". Received `;
        if (isError(actual)) {
          const name = (actual.constructor?.name) ||
                       actual.name;
          if (expected.name === name) {
            message += 'an error with identical name but a different prototype.';
          } else {
            message += `"${name}"`;
          }
          if (actual.message) {
            message += `\n\nError message:\n\n${actual.message}`;
          }
        } else {
          message += `"${inspect(actual, { depth: -1 })}"`;
        }
      }
      throwError = true;
    } else {
      // Check validation functions return value.
      const res = ReflectApply(expected, {}, [actual]);
      if (res !== true) {
        if (!message) {
          generatedMessage = true;
          const name = expected.name ? `"${expected.name}" ` : '';
          message = `The ${name}validation function is expected to return` +
            ` "true". Received ${inspect(res)}`;

          if (isError(actual)) {
            message += `\n\nCaught error:\n\n${actual}`;
          }
        }
        throwError = true;
      }
    }

    if (throwError) {
      const err = new AssertionError(this.#buildAssertionErrorOptions({
        actual,
        expected,
        message,
        operator: fn.name,
        stackStartFn: fn,
      }));
      err.generatedMessage = generatedMessage;
      throw err;
    }
  }

  #expectsError(stackStartFn, actual, error, message) {
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
            `The error message "${actual.message}" is identical to the message.`,
          );
        }
      } else if (actual === error) {
        throw new ERR_AMBIGUOUS_ARGUMENT(
          'error/message',
          `The error "${actual}" is identical to the message.`,
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
      if (error?.name) {
        details += ` (${error.name})`;
      }
      details += message ? `: ${message}` : '.';
      const fnType = stackStartFn === Assert.prototype.rejects ? 'rejection' : 'exception';
      this.#innerFail({
        actual: undefined,
        expected: error,
        operator: stackStartFn.name,
        message: `Missing expected ${fnType}${details}`,
        stackStartFn,
      });
    }

    if (!error)
      return;

    this.#expectedException(actual, error, message, stackStartFn);
  }

  #expectsNoError(stackStartFn, actual, error, message) {
    if (actual === NO_EXCEPTION_SENTINEL)
      return;

    if (typeof error === 'string') {
      message = error;
      error = undefined;
    }

    if (!error || this.#hasMatchingError(actual, error)) {
      const details = message ? `: ${message}` : '.';
      const fnType = stackStartFn === Assert.prototype.doesNotReject ?
        'rejection' : 'exception';
      this.#innerFail({
        actual,
        expected: error,
        operator: stackStartFn.name,
        message: `Got unwanted ${fnType}${details}\n` +
                 `Actual message: "${actual?.message}"`,
        stackStartFn,
      });
    }
    throw actual;
  }

  #hasMatchingError(actual, expected) {
    if (typeof expected !== 'function') {
      if (isRegExp(expected)) {
        const str = String(actual);
        return RegExpPrototypeExec(expected, str) !== null;
      }
      throw new ERR_INVALID_ARG_TYPE(
        'expected', ['Function', 'RegExp'], expected,
      );
    }
    // Guard instanceof against arrow functions as they don't have a prototype.
    if (expected.prototype !== undefined && actual instanceof expected) {
      return true;
    }
    if (ObjectPrototypeIsPrototypeOf(Error, expected)) {
      return false;
    }
    return ReflectApply(expected, {}, [actual]) === true;
  }

  async #waitForActual(promiseFn) {
    let resultPromise;
    if (typeof promiseFn === 'function') {
      // Return a rejected promise if `promiseFn` throws synchronously.
      resultPromise = promiseFn();
      // Fail in case no promise is returned.
      if (!this.#checkIsPromise(resultPromise)) {
        throw new ERR_INVALID_RETURN_VALUE('instance of Promise',
                                           'promiseFn', resultPromise);
      }
    } else if (this.#checkIsPromise(promiseFn)) {
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

  #getActual(fn) {
    validateFunction(fn, 'fn');
    try {
      fn();
    } catch (e) {
      return e;
    }
    return NO_EXCEPTION_SENTINEL;
  }

  #checkIsPromise(obj) {
    // Accept native ES6 promises and promises that are implemented in a similar
    // way. Do not accept thenables that use a function as `obj` and that have no
    // `catch` handler.
    return isPromise(obj) ||
      (obj !== null && typeof obj === 'object' &&
      typeof obj.then === 'function' &&
      typeof obj.catch === 'function');
  }

  /**
   * Pure assertion tests whether a value is truthy, as determined
   * by !!value.
   * @param {...any} args
   * @returns {void}
   */
  ok(...args) {
    innerOk(this.ok, args.length, ...args);
  }

  /**
   * Throws an AssertionError with the given message.
   * @param {any | Error} [message]
   */
  fail(message) {
    if (isError(message)) throw message;

    let internalMessage = false;
    if (message === undefined) {
      message = 'Failed';
      internalMessage = true;
    }

    const errArgs = {
      operator: 'fail',
      stackStartFn: this.fail,
      message,
    };
    const err = new AssertionError(this.#buildAssertionErrorOptions(errArgs));
    if (internalMessage) {
      err.generatedMessage = true;
    }
    throw err;
  }

  /**
   * The equality assertion tests shallow, coercive equality with ==.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  equal(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    // eslint-disable-next-line eqeqeq
    if (actual != expected && (!NumberIsNaN(actual) || !NumberIsNaN(expected))) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: '==',
        stackStartFn: this.equal,
      });
    }
  };

  /**
   * The non-equality assertion tests for whether two objects are not
   * equal with !=.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  notEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    // eslint-disable-next-line eqeqeq
    if (actual == expected || (NumberIsNaN(actual) && NumberIsNaN(expected))) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: '!=',
        stackStartFn: this.notEqual,
      });
    }
  };

  /**
   * The deep equivalence assertion tests a deep equality relation.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  deepEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    if (!isDeepEqual(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'deepEqual',
        stackStartFn: this.deepEqual,
      });
    }
  };

  /**
   * The deep non-equivalence assertion tests for any deep inequality.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  notDeepEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    if (isDeepEqual(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'notDeepEqual',
        stackStartFn: this.notDeepEqual,
      });
    }
  };

  /**
   * The deep strict equivalence assertion tests a deep strict equality
   * relation.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  deepStrictEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    if (!isDeepStrictEqual(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'deepStrictEqual',
        stackStartFn: this.deepStrictEqual,
      });
    }
  };

  /**
   * The deep strict non-equivalence assertion tests for any deep strict
   * inequality.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  notDeepStrictEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    if (isDeepStrictEqual(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'notDeepStrictEqual',
        stackStartFn: this.notDeepStrictEqual,
      });
    }
  }

  /**
   * The strict equivalence assertion tests a strict equality relation.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  strictEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (!ObjectIs(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'strictEqual',
        stackStartFn: this.strictEqual,
      });
    }
  };

  /**
   * The strict non-equivalence assertion tests for any strict inequality.
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  notStrictEqual(actual, expected, message) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (ObjectIs(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'notStrictEqual',
        stackStartFn: this.notStrictEqual,
      });
    }
  };

  /**
   * The strict equivalence assertion test between two objects
   * @param {any} actual
   * @param {any} expected
   * @param {string | Error} [message]
   * @returns {void}
   */
  partialDeepStrictEqual(
    actual,
    expected,
    message,
  ) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('actual', 'expected');
    }
    if (isDeepEqual === undefined) lazyLoadComparison();
    if (!isPartialStrictEqual(actual, expected)) {
      this.#innerFail({
        actual,
        expected,
        message,
        operator: 'partialDeepStrictEqual',
        stackStartFn: this.partialDeepStrictEqual,
      });
    }
  };

  /**
   * Expects the `string` input to match the regular expression.
   * @param {string} string
   * @param {RegExp} regexp
   * @param {string | Error} [message]
   * @returns {void}
   */
  match(string, regexp, message) {
    this.#internalMatch(string, regexp, message, Assert.prototype.match);
  };

  /**
   * Expects the `string` input not to match the regular expression.
   * @param {string} string
   * @param {RegExp} regexp
   * @param {string | Error} [message]
   * @returns {void}
   */
  doesNotMatch(string, regexp, message) {
    this.#internalMatch(string, regexp, message, Assert.prototype.doesNotMatch);
  };

  /**
   * Expects the function `promiseFn` to throw an error.
   * @param {() => any} promiseFn
   * @param {...any} [args]
   * @returns {void}
   */
  throws(promiseFn, ...args) {
    this.#expectsError(Assert.prototype.throws, this.#getActual(promiseFn), ...args);
  };

  /**
   * Expects `promiseFn` function or its value to reject.
   * @param {() => Promise<any>} promiseFn
   * @param {...any} [args]
   * @returns {Promise<void>}
   */
  async rejects(promiseFn, ...args) {
    this.#expectsError(Assert.prototype.rejects, await this.#waitForActual(promiseFn), ...args);
  };

  /**
   * Asserts that the function `fn` does not throw an error.
   * @param {() => any} fn
   * @param {...any} [args]
   * @returns {void}
   */
  doesNotThrow(fn, ...args) {
    this.#expectsNoError(Assert.prototype.doesNotThrow, this.#getActual(fn), ...args);
  };

  /**
   * Expects `fn` or its value to not reject.
   * @param {() => Promise<any>} fn
   * @param {...any} [args]
   * @returns {Promise<void>}
   */
  async doesNotReject(fn, ...args) {
    this.#expectsNoError(Assert.prototype.doesNotReject, await this.#waitForActual(fn), ...args);
  };

  /**
   * Throws `value` if the value is not `null` or `undefined`.
   * @param {any} err
   * @returns {void}
   */
  ifError(err) {
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

      const newErr = new AssertionError(this.#buildAssertionErrorOptions({
        actual: err,
        expected: null,
        operator: 'ifError',
        message,
        stackStartFn: this.ifError,
      }));

      // Make sure we actually have a stack trace!
      const origStack = err.stack;

      if (typeof origStack === 'string') {
        // This will remove any duplicated frames from the error frames taken
        // from within `ifError` and add the original error frames to the newly
        // created ones.
        const origStackStart = StringPrototypeIndexOf(origStack, '\n    at');
        if (origStackStart !== -1) {
          const originalFrames = StringPrototypeSplit(
            StringPrototypeSlice(origStack, origStackStart + 1),
            '\n',
          );
          // Filter all frames existing in err.stack.
          let newFrames = StringPrototypeSplit(newErr.stack, '\n');
          for (const errFrame of originalFrames) {
            // Find the first occurrence of the frame.
            const pos = ArrayPrototypeIndexOf(newFrames, errFrame);
            if (pos !== -1) {
              // Only keep new frames.
              newFrames = ArrayPrototypeSlice(newFrames, 0, pos);
              break;
            }
          }
          const stackStart = ArrayPrototypeJoin(newFrames, '\n');
          const stackEnd = ArrayPrototypeJoin(originalFrames, '\n');
          newErr.stack = `${stackStart}\n${stackEnd}`;
        }
      }

      throw newErr;
    }
  };
}

const assertInstance = new Assert();
['ok', 'fail', 'equal', 'notEqual', 'deepEqual', 'notDeepEqual',
 'deepStrictEqual', 'notDeepStrictEqual', 'strictEqual',
 'notStrictEqual', 'partialDeepStrictEqual', 'match', 'doesNotMatch',
 'throws', 'rejects', 'doesNotThrow', 'doesNotReject', 'ifError'].forEach((name) => {
  assertInstance[name] = assertInstance[name].bind(assertInstance);
});

/**
 * Pure assertion tests whether a value is truthy, as determined
 * by !!value.
 * @param {...any} args
 * @returns {void}
 */
function ok(...args) {
  innerOk(ok, args.length, ...args);
}
ObjectAssign(assert, assertInstance);
assert.ok = ok;

/**
 * Expose a strict only variant of assert.
 * @param {...any} args
 * @returns {void}
 */
function strict(...args) {
  innerOk(strict, args.length, ...args);
}

assert.AssertionError = AssertionError;

assert.strict = ObjectAssign(strict, assert, {
  equal: assert.strictEqual,
  deepEqual: assert.deepStrictEqual,
  notEqual: assert.notStrictEqual,
  notDeepEqual: assert.notDeepStrictEqual,
});

assert.strict.Assert = Assert;
assert.strict.strict = assert.strict;

module.exports.Assert = Assert;
