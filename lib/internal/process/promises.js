'use strict';

const { safeToString } = process.binding('util');

const promiseRejectEvent = process._promiseRejectEvent;
const hasBeenNotifiedProperty = new WeakMap();
const promiseToGuidProperty = new WeakMap();
const pendingUnhandledRejections = [];
let lastPromiseId = 1;

exports.setup = setupPromises;

function getAsynchronousRejectionWarningObject(uid) {
  return new Error('Promise rejection was handled ' +
                   `asynchronously (rejection id: ${uid})`);
}

function setupPromises(scheduleMicrotasks) {
  process._setupPromises(function(event, promise, reason) {
    if (event === promiseRejectEvent.unhandled)
      unhandledRejection(promise, reason);
    else if (event === promiseRejectEvent.handled)
      rejectionHandled(promise);
    else
      require('assert').fail(null, null, 'unexpected PromiseRejectEvent');
  });

  function unhandledRejection(promise, reason) {
    hasBeenNotifiedProperty.set(promise, false);
    promiseToGuidProperty.set(promise, lastPromiseId++);
    addPendingUnhandledRejection(promise, reason);
  }

  function rejectionHandled(promise) {
    const hasBeenNotified = hasBeenNotifiedProperty.get(promise);
    if (hasBeenNotified !== undefined) {
      hasBeenNotifiedProperty.delete(promise);
      const uid = promiseToGuidProperty.get(promise);
      promiseToGuidProperty.delete(promise);
      if (hasBeenNotified === true) {
        let warning = null;
        if (!process.listenerCount('rejectionHandled')) {
          // Generate the warning object early to get a good stack trace.
          warning = getAsynchronousRejectionWarningObject(uid);
        }
        process.nextTick(function() {
          if (!process.emit('rejectionHandled', promise)) {
            if (warning === null)
              warning = getAsynchronousRejectionWarningObject(uid);
            warning.name = 'PromiseRejectionHandledWarning';
            warning.id = uid;
            process.emitWarning(warning);
          }
        });
      }

    }
  }

  function emitPendingUnhandledRejections() {
    let hadListeners = false;
    while (pendingUnhandledRejections.length > 0) {
      const promise = pendingUnhandledRejections.shift();
      const reason = pendingUnhandledRejections.shift();
      if (hasBeenNotifiedProperty.get(promise) === false) {
        hasBeenNotifiedProperty.set(promise, true);
        const uid = promiseToGuidProperty.get(promise);
        if (!process.emit('unhandledRejection', reason, promise)) {
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
