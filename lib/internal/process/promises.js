'use strict';

const { ERR_INVALID_ENV_VALUE } = require('internal/errors').codes;

// TODO: Do not add stack traces to the here created errors as they are
// meaningless. That improves the performance in error cases.
// TODO: Make the error names non-enumerable.

const { safeToString } = process.binding('util');

const maybeUnhandledPromises = new WeakMap();

let pendingUnhandledRejections = [];
let asyncHandledRejections = [];
let lastPromiseId = 0;

const kFromPromise = true;
const kFromError = false;
const states = {
  SILENT: 0,
  WARN: 1,
  ERROR_ON_GC: 2,
  ERROR: 3,
};

function unhandledState() {
  let env = process.env.NODE_UNHANDLED_REJECTION;
  if (env) {
    env = env.toUpperCase();
    if (states[env] !== undefined) {
      return states[env];
    }
    // Raise an exception to indicate the faulty environment variable value.
    const err = new ERR_INVALID_ENV_VALUE(env, 'NODE_UNHANDLED_REJECTION');
    fatalException(err, kFromError);
    // Remove the faulty environment variable to make sure everything works fine
    // afterwards if the error is handled by the user and the user decided to
    // continue running the application.
    delete process.env.NODE_UNHANDLED_REJECTION;
  }
  return states.WARN;
}

function setupPromises(_setupPromises) {
  // Verify while startup that a possible environment variable has a valid
  // value.
  unhandledState();
  _setupPromises(unhandledRejection, handledRejection);
  return emitPromiseRejectionWarnings;
}

function unhandledRejection(promise, reason) {
  maybeUnhandledPromises.set(promise, {
    reason,
    uid: ++lastPromiseId,
    warned: false
  });
  // This causes the promise to be referenced at least for one tick.
  pendingUnhandledRejections.push(promise);
  return true;
}

function handledRejection(promise) {
  const promiseInfo = maybeUnhandledPromises.get(promise);
  if (promiseInfo !== undefined) {
    maybeUnhandledPromises.delete(promise);
    if (promiseInfo.warned) {
      const { uid } = promiseInfo;
      // Generate the warning object early to get a good stack trace.
      // eslint-disable-next-line no-restricted-syntax
      const warning = new Error('Promise rejection was handled ' +
                                `asynchronously (rejection id: ${uid})`);
      warning.name = 'PromiseRejectionHandledWarning';
      warning.id = uid;
      asyncHandledRejections.push({ promise, warning });
      return true;
    }
  }
  return false;
}

const unhandledRejectionErrName = 'UnhandledPromiseRejectionWarning';
function emitWarning(uid, reason) {
  try {
    if (reason instanceof Error) {
      process.emitWarning(reason.stack, unhandledRejectionErrName);
    } else {
      process.emitWarning(safeToString(reason), unhandledRejectionErrName);
    }
  } catch {}
  // TODO: Reword the error message. It partially duplicates the error name.
  // TODO: Add a code properly to the error.
  // eslint-disable-next-line no-restricted-syntax
  const warning = new Error(
    'Unhandled promise rejection. This error originated either by ' +
    'throwing inside of an async function without a catch block, ' +
    'or by rejecting a promise which was not handled with .catch(). ' +
    `(rejection id: ${uid})`
  );
  warning.name = unhandledRejectionErrName;
  try {
    if (reason instanceof Error) {
      warning.stack = reason.stack;
    }
  } catch {}
  process.emitWarning(warning);
  // Only emit the deprecation notice in case the user did not opt into using
  // the environment variable / flag.
  if (!process.env.NODE_UNHANDLED_REJECTION) {
    emitDeprecationWarning();
  }
}

let deprecationWarned = false;
function emitDeprecationWarning() {
  if (!deprecationWarned) {
    deprecationWarned = true;
    process.emitWarning(
      'Unhandled promise rejections are deprecated. In the future, ' +
      'promise rejections that are not handled will terminate the ' +
      'Node.js process with a non-zero exit code.',
      'DeprecationWarning', 'DEP0018');
  }
}

function emitPromiseRejectionWarnings() {
  while (asyncHandledRejections.length > 0) {
    const { promise, warning } = asyncHandledRejections.shift();
    if (!process.emit('rejectionHandled', promise)) {
      process.emitWarning(warning);
    } else {
      // Stop the loop early.
      asyncHandledRejections = [];
    }
  }

  let len = pendingUnhandledRejections.length;
  while (len--) {
    const promise = pendingUnhandledRejections.shift();
    const promiseInfo = maybeUnhandledPromises.get(promise);
    if (promiseInfo === undefined) {
      continue;
    }
    promiseInfo.warned = true;
    const { reason, uid } = promiseInfo;
    const state = unhandledState();
    if (state === states.ERROR) {
      fatalException(reason, kFromPromise);
    }
    if (!process.emit('unhandledRejection', reason, promise)) {
      if (state === states.WARN || state === states.ERROR_ON_GC) {
        emitWarning(uid, reason);
      // Stop the loop as soon as possible. Since the handler might have been
      // attached in the beginning and then removed again and a new unhandled
      // rejection might have been added at that former state, only stop if no
      // new rejections were added.
      } else if (len === pendingUnhandledRejections.length &&
          state < states.ERROR) {
        pendingUnhandledRejections = [];
        break;
      }
    }
  }
  return pendingUnhandledRejections.length !== 0;
}

function createError(name, message) {
  // Reset the stack to prevent any overhead.
  const tmp = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(message);
  Error.stackTraceLimit = tmp;
  Object.defineProperty(err, 'name', {
    value: name,
    writable: true,
    configurable: true,
  });
  return err;
}

function fatalException(reason, fromPromise) {
  let err;
  if (reason instanceof Error) {
    err = reason;
  } else {
    err = createError(
      'UnhandledPromiseRejection',
      'This error originated either by ' +
        'throwing inside of an async function without a catch block, ' +
        'or by rejecting a promise which was not handled with .catch().' +
        ` The promise rejected with the reason "${safeToString(reason)}".`
    );
  }
  // Throw an error if the user did not handle the exception.
  if (!process._fatalException(err, fromPromise)) {
    process._rawDebug(err.stack);
    if (process.binding('config').shouldAbortOnUncaughtException) {
      process.abort();
    }
    process.exit(1);
  }
}

module.exports = {
  setup: setupPromises,
  unhandledState,
  states,
};
