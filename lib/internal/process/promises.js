'use strict';

const { safeToString } = process.binding('util');

const maybeUnhandledPromises = new WeakMap();
const pendingUnhandledRejections = [];
const asyncHandledRejections = [];
let lastPromiseId = 0;

module.exports = {
  emitPromiseRejectionWarnings
};

process._setupPromises(unhandledRejection, handledRejection);

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
  } catch (e) {
    // ignored
  }

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
  } catch (err) {
    // ignored
  }
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

  let hadListeners = false;
  let len = pendingUnhandledRejections.length;
  while (len--) {
    const promise = pendingUnhandledRejections.shift();
    const promiseInfo = maybeUnhandledPromises.get(promise);
    if (promiseInfo !== undefined) {
      promiseInfo.warned = true;
      const { reason, uid } = promiseInfo;
      if (!process.emit('unhandledRejection', reason, promise)) {
        emitWarning(uid, reason);
      } else {
        hadListeners = true;
      }
    }
  }
  return hadListeners || pendingUnhandledRejections.length !== 0;
}
