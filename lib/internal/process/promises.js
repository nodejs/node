'use strict';

const { safeToString } = process.binding('util');

exports.setup = function setup(scheduleMicrotasks) {
  const promiseRejectEvent = process._promiseRejectEvent;
  const hasBeenNotifiedProperty = new Map();
  const promiseToGuidProperty = new Map();
  const pendingUnhandledRejections = [];
  const promiseInternals = {};
  let lastPromiseId = 1;

  process._setupPromises(function(event, promise, reason) {
    if (event === promiseRejectEvent.unhandled)
      unhandledRejection(promise, reason);
    else if (event === promiseRejectEvent.handled)
      rejectionHandled(promise);
    else
      require('assert').fail(null, null, 'unexpected PromiseRejectEvent');
  }, function getPromiseReason(data) {
    // TODO: Remove this function as it should not be necessary.
    return data[3];
  }, promiseInternals);

  function unhandledRejection(promise, reason) {
    hasBeenNotifiedProperty.set(promise, false);
    promiseToGuidProperty.set(promise, lastPromiseId++);
    addPendingUnhandledRejection(promise, reason);
  }

  function rejectionHandled(promise) {
    const hasBeenNotified = hasBeenNotifiedProperty.get(promise);
    hasBeenNotifiedProperty.delete(promise);
    if (hasBeenNotified) {
      const uid = promiseToGuidProperty.get(promise);
      promiseToGuidProperty.delete(promise);
      let warning = null;
      if (!process.listenerCount('rejectionHandled')) {
      // Generate the warning object early to get a good stack trace.
        warning = new Error('Promise rejection was handled ' +
                                    `asynchronously (rejection id: ${uid})`);
      }
      promiseInternals.untrackPromise(promise);
      process.nextTick(function() {
        if (!process.emit('rejectionHandled', promise)) {
          if (warning === null)
            warning = new Error('Promise rejection was handled ' +
                                      `asynchronously (rejection id: ${uid})`);
          warning.name = 'PromiseRejectionHandledWarning';
          warning.id = uid;
          process.emitWarning(warning);
        }
      });
    } else {
      promiseToGuidProperty.delete(promise);
    }
  }

  function emitWarning(promise, reason) {
    const uid = promiseToGuidProperty.get(promise);
    const warning = new Error('Unhandled promise rejection ' +
                              `(rejection id: ${uid}): ${safeToString(reason)}`);
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
          emitWarning(promise, reason);
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
};
