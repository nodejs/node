'use strict';

const { safeToString } = process.binding('util');

const promiseRejectEvent = process._promiseRejectEvent;
const hasBeenNotifiedProperty = new WeakMap();
const pendingUnhandledRejections = [];

exports.setup = setupPromises;

function setupPromises(scheduleMicrotasks) {
  let deprecationWarned = false;
  const promiseInternals = {};

  process._setupPromises(function(event, promise, reason) {
    if (event === promiseRejectEvent.unhandled)
      unhandledRejection(promise, reason);
    else if (event === promiseRejectEvent.handled)
      rejectionHandled(promise);
    else
      require('assert').fail(null, null, 'unexpected PromiseRejectEvent');
  }, function getPromiseReason(data) {
    return data[data.indexOf('[[PromiseValue]]') + 1];
  }, promiseInternals);

  function unhandledRejection(promise, reason) {
    hasBeenNotifiedProperty.set(promise, false);
    addPendingUnhandledRejection(promise, reason);
  }

  function rejectionHandled(promise) {
    const hasBeenNotified = hasBeenNotifiedProperty.get(promise);
    if (hasBeenNotified !== undefined) {
      hasBeenNotifiedProperty.delete(promise);
      if (hasBeenNotified === true) {
        promiseInternals.untrackPromise(promise);
        process.nextTick(function() {
          process.emit('rejectionHandled', promise);
        });
      }

    }
  }

  function emitWarning(uid, reason) {
    const warning = new Error(
      `Unhandled promise rejection (rejection id: ${uid}): ` +
      safeToString(reason));
    warning.name = 'UnhandledPromiseRejectionWarning';
    warning.id = uid;
    try {
      if (reason instanceof Error) {
        warning.stack = reason.stack;
      }
    } catch (err) {
      // ignored
    }
    process.emitWarning(warning);
    if (!deprecationWarned) {
      deprecationWarned = true;
      process.emitWarning(
        'Unhandled promise rejections are deprecated. In the future, ' +
        'promise rejections that are not handled will terminate the ' +
        'Node.js process with a non-zero exit code.',
        'DeprecationWarning', 'DEP0018');
    }
  }

  function emitPendingUnhandledRejections() {
    let hadListeners = false;
    while (pendingUnhandledRejections.length > 0) {
      const promise = pendingUnhandledRejections.shift();
      const reason = pendingUnhandledRejections.shift();
      if (hasBeenNotifiedProperty.get(promise) === false) {
        hasBeenNotifiedProperty.set(promise, true);
        if (!process.emit('unhandledRejection', reason, promise)) {
          promiseInternals.trackPromise(promise);
        } else {
          hadListeners = true;
        }
      }
    }
    return hadListeners;
  }

  function addPendingUnhandledRejection(promise, reason) {
    pendingUnhandledRejections.push(promise, reason);
    scheduleMicrotasks();
  }

  return emitPendingUnhandledRejections;
}
