'use strict';

const promiseRejectEvent = process._promiseRejectEvent;
const hasBeenNotifiedProperty = new WeakMap();
const pendingUnhandledRejections = [];

exports.setup = setupPromises;

function setupPromises(scheduleMicrotasks) {
  const promiseInternals = {};

  process._setupPromises(function(event, promise, reason) {
    if (event === promiseRejectEvent.unhandled)
      unhandledRejection(promise, reason);
    else if (event === promiseRejectEvent.handled)
      rejectionHandled(promise);
    else
      require('assert').fail(null, null, 'unexpected PromiseRejectEvent');
  }, function getPromiseReason(data) {
    var valueindex = data.indexOf('[[PromiseValue]]');
    if (valueindex > 0) {
      return data[valueindex + 1];
    }
  }, promiseInternals);

  function unhandledRejection(promise, reason) {
    hasBeenNotifiedProperty.set(promise, false);
    addPendingUnhandledRejection(promise, reason);
  }

  function rejectionHandled(promise) {
    var hasBeenNotified = hasBeenNotifiedProperty.get(promise);
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

  function emitPendingUnhandledRejections() {
    var hadListeners = false;
    while (pendingUnhandledRejections.length > 0) {
      var promise = pendingUnhandledRejections.shift();
      var reason = pendingUnhandledRejections.shift();
      if (hasBeenNotifiedProperty.get(promise) === false) {
        hasBeenNotifiedProperty.set(promise, true);
        if (!process.emit('unhandledRejection', reason, promise)) {
          promiseInternals.onPromiseGC(promise);
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
