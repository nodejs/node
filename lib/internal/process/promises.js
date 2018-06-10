'use strict';

const { ERR_INVALID_ENV_VALUE } = require('internal/errors').codes;

const { safeToString } = process.binding('util');

const maybeUnhandledPromises = new WeakMap();
let pendingUnhandledRejections = [];
let asyncHandledRejections = [];
const kFromPromise = true;
let lastPromiseId = 0;

const kSilent = 0;
const kWarn = 1;
const kErrorOnGC = 2;
const kError = 3;

exports.setup = setupPromises;

function unhandledState() {
  let env = process.env.NODE_UNHANDLED_REJECTION;
  if (env) {
    env = env.toUpperCase();
    if (env === 'ERROR') {
      return kError;
    }
    if (env === 'SILENT') {
      return kSilent;
    }
    if (env === 'ERROR_ON_GC') {
      return kErrorOnGC;
    }
    if (env !== 'WARN') {
      throw new ERR_INVALID_ENV_VALUE(env, 'NODE_UNHANDLED_REJECTION');
    }
  }
  return kWarn;
}

function setupPromises(_setupPromises) {
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
}

function emitPromiseRejectionWarnings() {
  while (asyncHandledRejections.length > 0) {
    if (unhandledState() !== kError) {
      const { promise, warning } = asyncHandledRejections.shift();
      if (!process.emit('rejectionHandled', promise)) {
        process.emitWarning(warning);
      } else {
        // Stop the loop early.
        asyncHandledRejections = [];
        break;
      }
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
      const state = unhandledState();
      if (state === kError) {
        let err;
        try {
          if (reason instanceof Error) {
            err = reason;
          } else {
            // eslint-disable-next-line no-restricted-syntax
            err = new Error(
              'Unhandled promise rejection. This error originated either by ' +
              'throwing inside of an async function without a catch block, ' +
              'or by rejecting a promise which was not handled with .catch().' +
              ` The promise rejected with the reason "${safeToString(reason)}".`
            );
            err.name = 'UnhandledPromiseRejection';
          }
        } catch {}
        // Throw an error if the user did not handle the exception.
        if (!process._fatalException(err, kFromPromise)) {
          throw err;
        }
      } else if (!process.emit('unhandledRejection', reason, promise)) {
        if (state === kWarn || state === kErrorOnGC) {
          emitWarning(uid, reason);
        // Stop the loop as soon as possible. Since the handler might have been
        // attached in the beginning and then removed again and a new unhandled
        // rejection might have been added at that former state, only stop if no
        // new rejections were added.
        } else if (len === pendingUnhandledRejections.length) {
          pendingUnhandledRejections = [];
          break;
        }
      } else {
        hadListeners = true;
      }
    }
  }
  return hadListeners || pendingUnhandledRejections.length !== 0;
}
