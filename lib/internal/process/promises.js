'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  Error,
  ObjectDefineProperty,
  SafeWeakMap,
} = primordials;

const {
  tickInfo,
  promiseRejectEvents: {
    kPromiseRejectWithNoHandler,
    kPromiseHandlerAddedAfterReject,
    kPromiseResolveAfterResolved,
    kPromiseRejectAfterResolved
  },
  setPromiseRejectCallback
} = internalBinding('task_queue');

const {
  noSideEffectsToString,
  triggerUncaughtException
} = internalBinding('errors');

const {
  pushAsyncContext,
  popAsyncContext,
} = require('internal/async_hooks');
const async_hooks = require('async_hooks');
const { isErrorStackTraceLimitWritable } = require('internal/errors');

// *Must* match Environment::TickInfo::Fields in src/env.h.
const kHasRejectionToWarn = 1;

const maybeUnhandledPromises = new SafeWeakMap();
const pendingUnhandledRejections = [];
const asyncHandledRejections = [];
let lastPromiseId = 0;

// --unhandled-rejections=none:
// Emit 'unhandledRejection', but do not emit any warning.
const kIgnoreUnhandledRejections = 0;

// --unhandled-rejections=warn:
// Emit 'unhandledRejection', then emit 'UnhandledPromiseRejectionWarning'.
const kAlwaysWarnUnhandledRejections = 1;

// --unhandled-rejections=strict:
// Emit 'uncaughtException'. If it's not handled, print the error to stderr
// and exit the process.
// Otherwise, emit 'unhandledRejection'. If 'unhandledRejection' is not
// handled, emit 'UnhandledPromiseRejectionWarning'.
const kStrictUnhandledRejections = 2;

// --unhandled-rejections=throw:
// Emit 'unhandledRejection', if it's unhandled, emit
// 'uncaughtException'. If it's not handled, print the error to stderr
// and exit the process.
const kThrowUnhandledRejections = 3;

// --unhandled-rejections=warn-with-error-code:
// Emit 'unhandledRejection', if it's unhandled, emit
// 'UnhandledPromiseRejectionWarning', then set process exit code to 1.

const kWarnWithErrorCodeUnhandledRejections = 4;

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
      return kStrictUnhandledRejections;
    case 'throw':
      return kThrowUnhandledRejections;
    case 'warn-with-error-code':
      return kWarnWithErrorCodeUnhandledRejections;
    default:
      return kThrowUnhandledRejections;
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
  const asyncId = async_hooks.executionAsyncId();
  const triggerAsyncId = async_hooks.triggerAsyncId();
  const resource = promise;

  const emit = (reason, promise, promiseInfo) => {
    try {
      pushAsyncContext(asyncId, triggerAsyncId, resource);
      if (promiseInfo.domain) {
        return promiseInfo.domain.emit('error', reason);
      }
      return process.emit('unhandledRejection', reason, promise);
    } finally {
      popAsyncContext(asyncId);
    }
  };

  maybeUnhandledPromises.set(promise, {
    reason,
    uid: ++lastPromiseId,
    warned: false,
    domain: process.domain,
    emit
  });
  // This causes the promise to be referenced at least for one tick.
  ArrayPrototypePush(pendingUnhandledRejections, promise);
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
      ArrayPrototypePush(asyncHandledRejections, { promise, warning });
      setHasRejectionToWarn(true);
      return;
    }
  }
  if (maybeUnhandledPromises.size === 0 && asyncHandledRejections.length === 0)
    setHasRejectionToWarn(false);
}

const unhandledRejectionErrName = 'UnhandledPromiseRejectionWarning';
function emitUnhandledRejectionWarning(uid, reason) {
  const warning = getErrorWithoutStack(
    unhandledRejectionErrName,
    'Unhandled promise rejection. This error originated either by ' +
      'throwing inside of an async function without a catch block, ' +
      'or by rejecting a promise which was not handled with .catch(). ' +
      'To terminate the node process on unhandled promise ' +
      'rejection, use the CLI flag `--unhandled-rejections=strict` (see ' +
      'https://nodejs.org/api/cli.html#cli_unhandled_rejections_mode). ' +
      `(rejection id: ${uid})`
  );
  try {
    if (reason instanceof Error) {
      warning.stack = reason.stack;
      process.emitWarning(reason.stack, unhandledRejectionErrName);
    } else {
      process.emitWarning(
        noSideEffectsToString(reason), unhandledRejectionErrName);
    }
  } catch {}

  process.emitWarning(warning);
}

// If this method returns true, we've executed user code or triggered
// a warning to be emitted which requires the microtask and next tick
// queues to be drained again.
function processPromiseRejections() {
  let maybeScheduledTicksOrMicrotasks = asyncHandledRejections.length > 0;

  while (asyncHandledRejections.length > 0) {
    const { promise, warning } = ArrayPrototypeShift(asyncHandledRejections);
    if (!process.emit('rejectionHandled', promise)) {
      process.emitWarning(warning);
    }
  }

  let len = pendingUnhandledRejections.length;
  while (len--) {
    const promise = ArrayPrototypeShift(pendingUnhandledRejections);
    const promiseInfo = maybeUnhandledPromises.get(promise);
    if (promiseInfo === undefined) {
      continue;
    }
    promiseInfo.warned = true;
    const { reason, uid, emit } = promiseInfo;

    switch (unhandledRejectionsMode) {
      case kStrictUnhandledRejections: {
        const err = reason instanceof Error ?
          reason : generateUnhandledRejectionError(reason);
        triggerUncaughtException(err, true /* fromPromise */);
        const handled = emit(reason, promise, promiseInfo);
        if (!handled) emitUnhandledRejectionWarning(uid, reason);
        break;
      }
      case kIgnoreUnhandledRejections: {
        emit(reason, promise, promiseInfo);
        break;
      }
      case kAlwaysWarnUnhandledRejections: {
        emit(reason, promise, promiseInfo);
        emitUnhandledRejectionWarning(uid, reason);
        break;
      }
      case kThrowUnhandledRejections: {
        const handled = emit(reason, promise, promiseInfo);
        if (!handled) {
          const err = reason instanceof Error ?
            reason : generateUnhandledRejectionError(reason);
          triggerUncaughtException(err, true /* fromPromise */);
        }
        break;
      }
      case kWarnWithErrorCodeUnhandledRejections: {
        const handled = emit(reason, promise, promiseInfo);
        if (!handled) {
          emitUnhandledRejectionWarning(uid, reason);
          process.exitCode = 1;
        }
        break;
      }
    }
    maybeScheduledTicksOrMicrotasks = true;
  }
  return maybeScheduledTicksOrMicrotasks ||
         pendingUnhandledRejections.length !== 0;
}

function getErrorWithoutStack(name, message) {
  // Reset the stack to prevent any overhead.
  const tmp = Error.stackTraceLimit;
  if (isErrorStackTraceLimitWritable()) Error.stackTraceLimit = 0;
  // eslint-disable-next-line no-restricted-syntax
  const err = new Error(message);
  if (isErrorStackTraceLimitWritable()) Error.stackTraceLimit = tmp;
  ObjectDefineProperty(err, 'name', {
    value: name,
    enumerable: false,
    writable: true,
    configurable: true,
  });
  return err;
}

function generateUnhandledRejectionError(reason) {
  const message =
    'This error originated either by ' +
    'throwing inside of an async function without a catch block, ' +
    'or by rejecting a promise which was not handled with .catch().' +
    ' The promise rejected with the reason ' +
    `"${noSideEffectsToString(reason)}".`;

  const err = getErrorWithoutStack('UnhandledPromiseRejection', message);
  err.code = 'ERR_UNHANDLED_REJECTION';
  return err;
}

function listenForRejections() {
  setPromiseRejectCallback(promiseRejectHandler);
}
module.exports = {
  hasRejectionToWarn,
  setHasRejectionToWarn,
  listenForRejections,
  processPromiseRejections,
};
