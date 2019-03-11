'use strict';

const { safeToString } = internalBinding('util');

const maybeUnhandledPromises = new WeakMap();
const pendingUnhandledRejections = [];
const asyncHandledRejections = [];
const promiseRejectEvents = {};
let lastPromiseId = 0;

exports.setup = setupPromises;

function setupPromises(_setupPromises) {
  _setupPromises(promiseRejectHandler, promiseRejectEvents);
  return emitPromiseRejectionWarnings;
}

const states = {
  none: 0,
  warn: 1,
  strict: 2,
  default: 3
};

let state;

function promiseRejectHandler(type, promise, reason) {
  if (state === undefined) {
    const { getOptionValue } = require('internal/options');
    state = states[getOptionValue('--unhandled-rejections') || 'default'];
  }
  switch (type) {
    case promiseRejectEvents.kPromiseRejectWithNoHandler:
      return unhandledRejection(promise, reason);
    case promiseRejectEvents.kPromiseHandlerAddedAfterReject:
      return handledRejection(promise);
    case promiseRejectEvents.kPromiseResolveAfterResolved:
      return resolveError('resolve', promise, reason);
    case promiseRejectEvents.kPromiseRejectAfterResolved:
      return resolveError('reject', promise, reason);
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
  if (state === states.none) {
    return;
  }
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
  if (state === states.default && !deprecationWarned) {
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
    }
  }

  let maybeScheduledTicks = false;
  let len = pendingUnhandledRejections.length;
  while (len--) {
    const promise = pendingUnhandledRejections.shift();
    const promiseInfo = maybeUnhandledPromises.get(promise);
    if (promiseInfo === undefined) {
      continue;
    }
    promiseInfo.warned = true;
    const { reason, uid } = promiseInfo;
    if (state === states.strict) {
      fatalException(reason);
    }
    if (!process.emit('unhandledRejection', reason, promise) ||
        // Always warn in case the user requested it.
        state === states.warn) {
      emitWarning(uid, reason);
    }
    maybeScheduledTicks = true;
  }
  return maybeScheduledTicks || pendingUnhandledRejections.length !== 0;
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

  if (!process._fatalException(err, true /* fromPromise */)) {
    throw err;
  }
}
