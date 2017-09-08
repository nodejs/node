'use strict';

const promiseRejectEvent = process._promiseRejectEvent;

const kNotified = Symbol('node-unhandledrejection-notified');
const kGuid = Symbol('node-unhandledrejection-uid');
let lastPromiseId = 1;

class RejectedList {
  push(promise, reason) {
    if (this.tail === undefined) {
      this.head = this.tail = {promise, reason, next: undefined};
    } else {
      this.tail.next = {promise, reason, next: undefined};
      this.tail = this.tail.next;
    }
  }

  shift() {
    var item = this.head;
    if (item === undefined)
      return;
    this.head = item.next;
    if (this.head === undefined)
      this.tail = undefined;
    return item;
  }
}

const pendingUnhandledRejections = new RejectedList();

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
    Object.defineProperties(promise, {
      [kNotified]: {
        enumerable: false,
        configurable: true,
        writable: true,
        value: false
      },
      [kGuid]: {
        enumerable: false,
        configurable: true,
        value: lastPromiseId++
      }
    });
    addPendingUnhandledRejection(promise, reason);
  }

  function rejectionHandled(promise) {
    const hasBeenNotified = promise[kNotified];
    if (hasBeenNotified !== undefined) {
      promise[kNotified] = undefined;
      const uid = promise[kGuid];
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
                              `(rejection id: ${uid})`);
    warning.name = 'UnhandledPromiseRejectionWarning';
    warning.id = uid;
    if (reason instanceof Error) {
      warning.detail = reason.stack;
    }
    warning.reason = reason;
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
    let item;
    while ((item = pendingUnhandledRejections.shift()) !== undefined) {
      const { promise, reason } = item;
      if (promise[kNotified] === false) {
        promise[kNotified] = true;
        const uid = promise[kGuid];
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

exports.setup = setupPromises;
