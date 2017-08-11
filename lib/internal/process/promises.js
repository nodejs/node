'use strict';

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
  let deprecationWarned = false;

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

  function emitWarning(uid, reason) {
    const warning = new Error('Unhandled promise rejection ' +
                              `(rejection id: ${uid}): ${String(reason)}`);
    warning.name = 'UnhandledPromiseRejectionWarning';
    warning.id = uid;
    if (reason instanceof Error) {
      warning.stack = reason.stack;
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
        const uid = promiseToGuidProperty.get(promise);
        if (!process.emit('unhandledRejection', reason, promise)) {
          emitWarning(uid, reason);
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
