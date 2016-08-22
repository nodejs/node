'use strict';

const promiseRejectEvent = process._promiseRejectEvent;
const hasBeenNotifiedProperty = new WeakMap();
const promiseToGuidProperty = new WeakMap();
const pendingUnhandledRejections = [];
let lastPromiseId = 1;

exports.setup = setupPromises;

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
    var hasBeenNotified = hasBeenNotifiedProperty.get(promise);
    if (hasBeenNotified !== undefined) {
      hasBeenNotifiedProperty.delete(promise);
      const uid = promiseToGuidProperty.get(promise);
      promiseToGuidProperty.delete(promise);
      if (hasBeenNotified === true) {
        process.nextTick(function() {
          if (!process.emit('rejectionHandled', promise)) {
            const warning = new Error('Promise rejection was handled ' +
                                      `asynchronously (rejection id: ${uid})`);
            warning.name = 'PromiseRejectionHandledWarning';
            warning.id = uid;
            process.emitWarning(warning);
          }
        });
      }

    }
  }

  var deprecationWarned = false;
  function emitPendingUnhandledRejections() {
    let hadListeners = false;
    while (pendingUnhandledRejections.length > 0) {
      const promise = pendingUnhandledRejections.shift();
      const reason = pendingUnhandledRejections.shift();
      if (hasBeenNotifiedProperty.get(promise) === false) {
        hasBeenNotifiedProperty.set(promise, true);
        const uid = promiseToGuidProperty.get(promise);
        if (!process.emit('unhandledRejection', reason, promise)) {
          const warning = new Error('Unhandled promise rejection ' +
                                    `(rejection id: ${uid}): ${reason}`);
          warning.name = 'UnhandledPromiseRejectionWarning';
          warning.id = uid;
          process.emitWarning(warning);
          if (!deprecationWarned) {
            deprecationWarned = true;
            process.emitWarning(
              'Unhandled promise rejections are deprecated. In the future, ' +
              'promise rejections that are not handled will terminate the ' +
              'Node.js process with a non-zero exit code.',
              'DeprecationWarning');
          }
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
