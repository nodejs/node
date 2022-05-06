'use strict';

const {
  ArrayPrototypePush,
  Error,
  FunctionPrototype,
  Proxy,
  ReflectApply,
  SafeSet,
} = primordials;

const {
  codes: {
    ERR_UNAVAILABLE_DURING_EXIT,
  },
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const {
  validateUint32,
} = require('internal/validators');

const noop = FunctionPrototype;

class CallTracker {

  #callChecks = new SafeSet();

  calls(fn, exact = 1) {
    if (process._exiting)
      throw new ERR_UNAVAILABLE_DURING_EXIT();
    if (typeof fn === 'number') {
      exact = fn;
      fn = noop;
    } else if (fn === undefined) {
      fn = noop;
    }

    validateUint32(exact, 'exact', true);

    const context = {
      exact,
      actual: 0,
      // eslint-disable-next-line no-restricted-syntax
      stackTrace: new Error(),
      name: fn.name || 'calls'
    };
    const callChecks = this.#callChecks;
    callChecks.add(context);

    return new Proxy(fn, {
      __proto__: null,
      apply(fn, thisArg, argList) {
        context.actual++;
        if (context.actual === context.exact) {
          // Once function has reached its call count remove it from
          // callChecks set to prevent memory leaks.
          callChecks.delete(context);
        }
        // If function has been called more than expected times, add back into
        // callchecks.
        if (context.actual === context.exact + 1) {
          callChecks.add(context);
        }
        return ReflectApply(fn, thisArg, argList);
      },
    });
  }

  report() {
    const errors = [];
    for (const context of this.#callChecks) {
      // If functions have not been called exact times
      if (context.actual !== context.exact) {
        const message = `Expected the ${context.name} function to be ` +
                        `executed ${context.exact} time(s) but was ` +
                        `executed ${context.actual} time(s).`;
        ArrayPrototypePush(errors, {
          message,
          actual: context.actual,
          expected: context.exact,
          operator: context.name,
          stack: context.stackTrace
        });
      }
    }
    return errors;
  }

  verify() {
    const errors = this.report();
    if (errors.length > 0) {
      throw new AssertionError({
        message: 'Function(s) were not called the expected number of times',
        details: errors,
      });
    }
  }
}

module.exports = CallTracker;
