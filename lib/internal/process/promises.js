'use strict';

const { safeToString } = internalBinding('util');

const maybeUnhandledPromises = new WeakMap();
const pendingUnhandledRejections = [];
const asyncHandledRejections = [];
const promiseRejectEvents = {};
let lastPromiseId = 0;

exports.setup = setupPromises;

function setupPromises(_setupPromises) {
  _setupPromises(handler, promiseRejectEvents);
  return emitPromiseRejectionWarnings;
}

function handler(type, promise, reason) {
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
    }
  }

  let maybeScheduledTicks = false;
  let len = pendingUnhandledRejections.length;
  while (len--) {
    const promise = pendingUnhandledRejections.shift();
    const promiseInfo = maybeUnhandledPromises.get(promise);
    if (promiseInfo !== undefined) {
      promiseInfo.warned = true;
      const { reason, uid } = promiseInfo;
      if (!process.emit('unhandledRejection', reason, promise)) {
        emitWarning(uid, reason);
      }
      maybeScheduledTicks = true;
    }
  }
  return maybeScheduledTicks || pendingUnhandledRejections.length !== 0;
}
