'use strict';

const {
  ArrayPrototypePush,
  ArrayIsArray,
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
  genericNodeError,
} = require('internal/errors');

const AssertionError = require('internal/assert/assertion_error');
const {
  validateUint32,
} = require('internal/validators');
const {
  isDeepStrictEqual,
} = require('internal/util/comparisons');

const noop = FunctionPrototype;

class CallTracker {

  #callChecks = new SafeSet();

  #calls(fn, exact, withArgs = []) {
    if (process._exiting)
      throw new ERR_UNAVAILABLE_DURING_EXIT();

    // When calls([arg1, arg2], ?1)
    if (ArrayIsArray(fn)) {
      exact = typeof withArgs === 'number' ? withArgs : exact;
      withArgs = fn;
      fn = noop;
    }

    // When calls(1)
    if (typeof fn === 'number') {
      exact = fn;
      fn = noop;
    }

    // When calls()
    if (fn === undefined) {
      fn = noop;
    }
    // Else calls(fn, 1, [])

    validateUint32(exact, 'exact', true);

    const context = {
      exact,
      actual: 0,
      expectedArgs: withArgs,
      currentFnArgs: [],
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

        // Only spy args if requested
        if (context.expectedArgs.length)
          context.currentFnArgs = argList;

        const containsExpectArgs = isDeepStrictEqual(context.currentFnArgs, context.expectedArgs);

        if (context.actual === context.exact && containsExpectArgs) {
          // Once function has reached its call count remove it from
          // callChecks set to prevent memory leaks.
          callChecks.delete(context);
        }
        // If function has been called more than expected times, add back into
        // callchecks.
        if (context.actual === context.exact + 1 && containsExpectArgs) {
          callChecks.add(context);
        }
        return ReflectApply(fn, thisArg, argList);
      },
    });
  }

  callsWith(fn, withArgs, exact = 1) {
    const expectedArgsWerePassed = ArrayIsArray(fn) ||
      ArrayIsArray(exact) ||
      ArrayIsArray(withArgs);

    if (!expectedArgsWerePassed) {
      const message = 'the [ withArgs ] param is required';

      throw new AssertionError({
        message,
        details: ArrayPrototypePush([], {
          message,
          operator: 'callsWith',
          stack: genericNodeError()
        })
      });
    }

    return this.#calls(fn, exact, withArgs);
  }

  calls(fn, exact = 1) {
    return this.#calls(fn, exact);
  }

  report() {
    const errors = [];
    for (const context of this.#callChecks) {

      const needsToCheckArgs = !!context.expectedArgs;

      const invalidArgs = needsToCheckArgs ?
        (context.currentFnArgs !== context.expectedArgs) :
        false;

      const msg = needsToCheckArgs ? [
        `with args (${context.expectedArgs}) `,
        ` with args (${context.currentFnArgs})`,
      ] : ['', ''];

      // If functions have not been called exact times and with correct arguments
      if (
        context.actual !== context.exact ||
        invalidArgs
      ) {
        const message = `Expected the ${context.name} function to be ` +
                        `executed ${context.exact} time(s) ${msg[0]}but was ` +
                        `executed ${context.actual} time(s)${msg[1]}.`;
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
