'use strict';

const { Object } = primordials;

const {
  safeToString
} = internalBinding('util');
const {
  tickInfo,
  promiseRejectEvents: {
    kPromiseRejectWithNoHandler,
    kPromiseHandlerAddedAfterReject,
    kPromiseResolveAfterResolved,
    kPromiseRejectAfterResolved
  },
  setPromiseRejectCallback,
  triggerFatalException
} = internalBinding('task_queue');

// *Must* match Environment::TickInfo::Fields in src/env.h.
const kHasRejectionToWarn = 1;

const maybeUnhandledPromises = new WeakMap();
const pendingUnhandledRejections = [];
const asyncHandledRejections = [];
let lastPromiseId = 0;

// --unhandled-rejection=none:
// Emit 'unhandledRejection', but do not emit any warning.
const kIgnoreUnhandledRejections = 0;
// --unhandled-rejection=warn:
// Emit 'unhandledRejection', then emit 'UnhandledPromiseRejectionWarning'.
const kAlwaysWarnUnhandledRejections = 1;
// --unhandled-rejection=strict:
// Emit 'uncaughtException'. If it's not handled, print the error to stderr
// and exit the process.
// Otherwise, emit 'unhandledRejection'. If 'unhandledRejection' is not
// handled, emit 'UnhandledPromiseRejectionWarning'.
const kThrowUnhandledRejections = 2;
// --unhandled-rejection is unset:
// Emit 'unhandledRejection', if it's handled, emit
// 'UnhandledPromiseRejectionWarning', then emit deprecation warning.
const kDefaultUnhandledRejections = 3;

let unhandledRejectionsMode;

function setHasRejectionToWarn(value) {
  tickInfo[kHasRejectionToWarn] = value ? 1 : 0;
}

function hasRejectionToWarn() {
  return tickInfo[kHasRejectionToWarn] === 1;
}

function getUnhandledRejectionsMode() {
  const { getOptionValue } = require('internal/options');
  switch (getOptionValue('--unhandled-rejections')) {
    case 'none':
      return kIgnoreUnhandledRejections;
    case 'warn':
      return kAlwaysWarnUnhandledRejections;
    case 'strict':
      return kThrowUnhandledRejections;
    default:
      return kDefaultUnhandledRejections;
  }
}

function promiseRejectHandler(type, promise, reason) {
  if (unhandledRejectionsMode === undefined) {
    unhandledRejectionsMode = getUnhandledRejectionsMode();
  }
  switch (type) {
    case kPromiseRejectWithNoHandler:
      unhandledRejection(promise, reason);
      break;
    case kPromiseHandlerAddedAfterReject:
      handledRejection(promise);
      break;
    case kPromiseResolveAfterResolved:
      resolveError('resolve', promise, reason);
      break;
    case kPromiseRejectAfterResolved:
      resolveError('reject', promise, reason);
      break;
  }
}

function resolveError(type, promise, reason) {
  // We have to wrap this in a next tick. Otherwise the error could be caught by
  // the executed promise.
  process.nextTick(() => {
    process.emit('multipleResolves', type, promise, reason);
  });
}

function unhandledRejection(promise, reason) {
  maybeUnhandledPromises.set(promise, {
    reason,
    uid: ++lastPromiseId,
    warned: false
  });
  // This causes the promise to be referenced at least for one tick.
  pendingUnhandledRejections.push(promise);
  setHasRejectionToWarn(true);
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
      setHasRejectionToWarn(true);
      return;
    }
  }
  setHasRejectionToWarn(false);
}

const unhandledRejectionErrName = 'UnhandledPromiseRejectionWarning';
function emitWarning(uid, reason) {
  const warning = getError(
    unhandledRejectionErrName,
    'Unhandled promise rejection. This error originated either by ' +
      'throwing inside of an async function without a catch block, ' +
      'or by rejecting a promise which was not handled with .catch(). ' +
      `(rejection id: ${uid})`
  );
  try {
    if (reason instanceof Error) {
      warning.stack = reason.stack;
      process.emitWarning(reason.stack, unhandledRejectionErrName);
    } else {
      process.emitWarning(safeToString(reason), unhandledRejectionErrName);
    }
  } catch {}

  process.emitWarning(warning);
  emitDeprecationWarning();
}

let deprecationWarned = false;
function emitDeprecationWarning() {
  if (unhandledRejectionsMode === kDefaultUnhandledRejections &&
      !deprecationWarned) {
    deprecationWarned = true;
    process.emitWarning(
      'Unhandled promise rejections are deprecated. In the future, ' +
      'promise rejections that are not handled will terminate the ' +
      'Node.js process with a non-zero exit code.',
      'DeprecationWarning', 'DEP0018');
  }
}

// If this method returns true, we've executed user code or triggered
// a warning to be emitted which requires the microtask and next tick
// queues to be drained again.
function processPromiseRejections() {
  let maybeScheduledTicksOrMicrotasks = asyncHandledRejections.length > 0;

  while (asyncHandledRejections.length > 0) {
    const { promise, warning } = asyncHandledRejections.shift();
    if (!process.emit('rejectionHandled', promise)) {
      process.emitWarning(warning);
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
    switch (unhandledRejectionsMode) {
      case kThrowUnhandledRejections: {
        fatalException(reason);
        const handled = process.emit('unhandledRejection', reason, promise);
        if (!handled) emitWarning(uid, reason);
        break;
      }
      case kIgnoreUnhandledRejections: {
        process.emit('unhandledRejection', reason, promise);
        break;
      }
      case kAlwaysWarnUnhandledRejections: {
        process.emit('unhandledRejection', reason, promise);
        emitWarning(uid, reason);
        break;
      }
      case kDefaultUnhandledRejections: {
        const handled = process.emit('unhandledRejection', reason, promise);
        if (!handled) emitWarning(uid, reason);
        break;
      }
    }
    maybeScheduledTicksOrMicrotasks = true;
  }
  return maybeScheduledTicksOrMicrotasks ||
         pendingUnhandledRejections.length !== 0;
}

function getError(name, message) {
  // Reset the stack to prevent any overhead.
  const tmp = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(message);
  Error.stackTraceLimit = tmp;
  Object.defineProperty(err, 'name', {
    value: name,
    enumerable: false,
    writable: true,
    configurable: true,
  });
  return err;
}

function fatalException(reason) {
  let err;
  if (reason instanceof Error) {
    err = reason;
  } else {
    err = getError(
      'UnhandledPromiseRejection',
      'This error originated either by ' +
        'throwing inside of an async function without a catch block, ' +
        'or by rejecting a promise which was not handled with .catch().' +
        ` The promise rejected with the reason "${safeToString(reason)}".`
    );
    err.code = 'ERR_UNHANDLED_REJECTION';
  }
  triggerFatalException(err, true /* fromPromise */);
}

function listenForRejections() {
  setPromiseRejectCallback(promiseRejectHandler);
}

module.exports = {
  hasRejectionToWarn,
  setHasRejectionToWarn,
  listenForRejections,
  processPromiseRejections
};
