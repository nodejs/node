'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  Error,
  ObjectDefineProperty,
  ObjectPrototypeHasOwnProperty,
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

const { deprecate } = require('internal/util');

const {
  noSideEffectsToString,
  triggerUncaughtException
} = internalBinding('errors');

const {
  pushAsyncContext,
  popAsyncContext,
  symbols: {
    async_id_symbol: kAsyncIdSymbol,
    trigger_async_id_symbol: kTriggerAsyncIdSymbol
  }
} = require('internal/async_hooks');
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

function isErrorLike(o) {
  return typeof o === 'object' &&
         o !== null &&
         ObjectPrototypeHasOwnProperty(o, 'stack');
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

const multipleResolvesDeprecate = deprecate(
  () => {},
  'The multipleResolves event has been deprecated.',
  'DEP0160'
);
function resolveError(type, promise, reason) {
  // We have to wrap this in a next tick. Otherwise the error could be caught by
  // the executed promise.
  process.nextTick(() => {
    if (process.emit('multipleResolves', type, promise, reason)) {
      multipleResolvesDeprecate();
    }
  });
}

function unhandledRejection(promise, reason) {
  const emit = (reason, promise, promiseInfo) => {
    if (promiseInfo.domain) {
      return promiseInfo.domain.emit('error', reason);
    }
    return process.emit('unhandledRejection', reason, promise);
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
    if (isErrorLike(reason)) {
      warning.stack = reason.stack;
      process.emitWarning(reason.stack, unhandledRejectionErrName);
    } else {
      process.emitWarning(
        noSideEffectsToString(reason), unhandledRejectionErrName);
    }
  } catch {
    try {
      process.emitWarning(
        noSideEffectsToString(reason), unhandledRejectionErrName);
    } catch {
      // Ignore.
    }
  }

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

    let needPop = true;
    const {
      [kAsyncIdSymbol]: promiseAsyncId,
      [kTriggerAsyncIdSymbol]: promiseTriggerAsyncId,
    } = promise;
    // We need to check if async_hooks are enabled
    // don't use enabledHooksExist as a Promise could
    // come from a vm.* context and not have an async id
    if (typeof promiseAsyncId !== 'undefined') {
      pushAsyncContext(
        promiseAsyncId,
        promiseTriggerAsyncId,
        promise
      );
    }
    try {
      switch (unhandledRejectionsMode) {
        case kStrictUnhandledRejections: {
          const err = isErrorLike(reason) ?
            reason : generateUnhandledRejectionError(reason);
          // This destroys the async stack, don't clear it after
          triggerUncaughtException(err, true /* fromPromise */);
          if (typeof promiseAsyncId !== 'undefined') {
            pushAsyncContext(
              promise[kAsyncIdSymbol],
              promise[kTriggerAsyncIdSymbol],
              promise
            );
          }
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
            const err = isErrorLike(reason) ?
              reason : generateUnhandledRejectionError(reason);
              // This destroys the async stack, don't clear it after
            triggerUncaughtException(err, true /* fromPromise */);
            needPop = false;
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
    } finally {
      if (needPop) {
        if (typeof promiseAsyncId !== 'undefined') {
          popAsyncContext(promiseAsyncId);
        }
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
    __proto__: null,
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
