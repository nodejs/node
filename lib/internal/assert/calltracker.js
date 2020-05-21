'use strict';

const {
  Error,
  MathMax,
  SafeSet,
} = primordials;

const {
  codes: {
    ERR_AMBIGUOUS_ARGUMENT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_OPT_VALUE,
    ERR_UNAVAILABLE_DURING_EXIT,
  },
} = require('internal/errors');
const AssertionError = require('internal/assert/assertion_error');
const {
  validateUint32,
} = require('internal/validators');

const callChecks = new SafeSet();
const trackedFunction = () => {};

function combineErrorStacks(origStack, newErr) {
  // Make sure we actually have a stack trace!
  if (typeof origStack === 'string') {
    // This will remove any duplicated frames from the error frames taken
    // from within `ifError` and add the original error frames to the newly
    // created ones.
    const tmp2 = origStack.split('\n');
    tmp2.shift();
    // Filter all frames existing in err.stack.
    let tmp1 = newErr.stack.split('\n');
    for (const errFrame of tmp2) {
      // Find the first occurrence of the frame.
      const pos = tmp1.indexOf(errFrame);
      if (pos !== -1) {
        // Only keep new frames.
        tmp1 = tmp1.slice(0, pos);
        break;
      }
    }
    newErr.stack = `${tmp1.join('\n')}\n${tmp2.join('\n')}`;
  }
}

function checkMismatchError(
  { actual, expected, stackObj, callFrames, mode, operator }) {
  if (actual < expected || (actual > expected && mode === 'exact')) {
    const exact = mode === 'exact' ? 'exactly' : 'at least';
    const err = new AssertionError({
      message: `Mismatched ${operator} function calls. ` +
              `Expected ${exact} ${expected}, actual ${actual}.`,
      actual,
      expected,
      operator,
      stackStartFn: checkMismatchError
    });
    err.generatedMessage = true;
    combineErrorStacks(stackObj.stack, err);
    const callFrequency = {};
    for (const frame of callFrames) {
      callFrequency[frame] = (callFrequency[frame] || 0) + 1;
    }
    err.callFrames = callFrequency;
    return err;
  }
}

function verifyTrackers() {
  const errors = [];
  const { stackTraceLimit } = Error;
  Error.stackTraceLimit = 0;
  const errorMapper = {
    mismatch: false,
    noVerify: false
  };
  for (const context of callChecks) {
    let error = checkMismatchError(context);
    if (error) {
      errors.push(error);
      errorMapper.mismatch = true;
    } else if (context.verifyOnExit === undefined) {
      // Override the original verify function that is called during exit to
      // notify the user about missed .verify() calls.
      error = new AssertionError({
        message: `The call tracked "${context.operator}" function was not ` +
                  'verified before exit.\nIf you want the function to ' +
                  'implicitly be verified on exit, set the "verifyOnExit" ' +
                  'option to "true"',
        actual: '.verify() has not been called',
        expected: '.verify() to be called before program exit',
        operator: 'On exit assert.trackCalls check'
      });
      error.generatedMessage = true;
      combineErrorStacks(context.stackObj.stack, error);
      errors.push(error);
      errorMapper.noVerify = true;
    }
  }
  if (errors.length) {
    if (errors.length === 1) {
      throw errors[0];
    }
    let message = 'Mismatched function calls and missed .verify() calls ' +
                  'detected on exit.';
    let actual = 'functions have not been verified or have a mismatched ' +
                 'function call';
    if (errorMapper.noVerify !== errorMapper.mismatch) {
      if (errorMapper.mismatch) {
        message = 'Mismatched function calls detected on exit.';
        actual = 'functions mismatched the expected number of function calls';
      } else {
        message = 'Missed .verify() calls detected on exit.';
        actual = 'functions did not call .verify()';
      }
    }
    const error = new AssertionError({
      message: `${message} Check the "errors" property for details.`,
      actual: `${errors.length} ${actual}`,
      expected: 'All functions to be called the correct number of times',
      operator: 'verifyTracker'
    });
    error.generatedMessage = true;
    error.errors = errors;
    Error.stackTraceLimit = stackTraceLimit;
    throw error;
  }
  Error.stackTraceLimit = stackTraceLimit;
}

function trackCalls(fn, expected) {
  if (typeof fn === 'number' || typeof fn === 'object') {
    if (expected !== undefined) {
      throw new ERR_AMBIGUOUS_ARGUMENT(
        'fn',
        '"fn" may not be a number or object if "expected" is also defined'
      );
    }
    expected = fn;
    fn = trackedFunction;
  } else if (fn === undefined) {
    fn = trackedFunction;
  }

  let mode = 'exact';
  let verifyOnExit;
  let failEarly = true;

  if (expected === undefined) {
    expected = 1;
  } else if (typeof expected === 'object' && expected !== null) {
    ({
      mode = 'exact',
      verifyOnExit,
      expected = 1,
      failEarly = true
    } = expected);

    if (verifyOnExit !== undefined && typeof verifyOnExit !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE(
        'options.verifyOnExit', 'boolean', verifyOnExit);
    }
    if (typeof mode !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.mode', 'string', mode);
    }
    mode = mode.toLowerCase();
    if (mode !== 'exact' && mode !== 'minimum') {
      throw new ERR_INVALID_OPT_VALUE(
        'option.mode', expected, 'Expected "exact" or "minimum"');
    }
    if (typeof failEarly !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE('options.failEarly', 'boolean', failEarly);
    }
  }

  // It is possible to synchronously track calls during exit. Prohibit using the
  // tracker in case the exit should be checked. That would not be possible.
  if (process._exiting && verifyOnExit !== false) {
    throw new ERR_UNAVAILABLE_DURING_EXIT();
  }

  validateUint32(expected, 'expected', true);

  const { stackTraceLimit } = Error;
  Error.stackTraceLimit = MathMax(10, stackTraceLimit);
  const stackObj = {};
  // eslint-disable-next-line no-restricted-syntax
  Error.captureStackTrace(stackObj, trackCalls);
  Error.stackTraceLimit = stackTraceLimit;

  const context = {
    actual: 0,
    expected,
    stackObj,
    callFrames: [],
    mode,
    operator: fn.name || '<anonymous call tracked function>',
    verifyOnExit,
  };

  function callTracker() {
    context.actual++;
    // Memorize the current call frame.
    const { stackTraceLimit } = Error;
    Error.stackTraceLimit = 2;
    const stackObject = {};
    // eslint-disable-next-line no-restricted-syntax
    Error.captureStackTrace(stackObject, callTracker);
    // TODO(BridgeAR): Investigate why we can't collect call frames during exit.
    // TODO(BridgeAR): Prevent issues in case `Error.prepareStackTrace` is used.
    const stack = typeof stackObject.stack === 'string' ?
      stackObject.stack :
      '';
    const frame = (stack.split('\n')[1] || 'Unknown call frame').trimLeft();
    context.callFrames.push(frame);
    Error.stackTraceLimit = stackTraceLimit;
    // If the function has been called more than the expected times, call
    // verify() to fail early.
    if (context.actual > expected && failEarly) {
      verify();
    }
    return fn.apply(this, arguments);
  }

  function verify() {
    callChecks.delete(context);
    if (callChecks.size === 0 && verifyOnExit !== false) {
      process.off('exit', verifyTrackers);
    }
    const error = checkMismatchError(context);
    if (error) {
      throw error;
    }
  }

  if (verifyOnExit === true) {
    callTracker.verify = function verify() {
      throw new AssertionError({ message: 'NOT IMPLEMENTED' });
    };
  } else {
    callTracker.verify = verify;
  }

  if (verifyOnExit !== false) {
    // Add the exit listener only once to avoid listener leak warnings
    if (callChecks.size === 0) {
      process.once('exit', verifyTrackers);
    }

    callChecks.add(context);
  }

  return callTracker;
}

module.exports = {
  trackCalls,
  combineErrorStacks
};
