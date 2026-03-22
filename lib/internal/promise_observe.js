'use strict';

const { observePromise: _observe } = internalBinding('task_queue');

/**
 * Observes a promise's fulfillment or rejection without suppressing
 * the `unhandledRejection` event. APM and diagnostics tools can use
 * this to be notified of promise outcomes while still allowing
 * `unhandledRejection` to fire if no real handler exists.
 *
 * @param {Promise} promise - The promise to observe.
 * @param {Function} [onFulfilled] - Called when the promise fulfills.
 * @param {Function} [onRejected] - Called when the promise rejects.
 */
function observePromise(promise, onFulfilled, onRejected) {
  _observe(
    promise,
    onFulfilled ?? (() => {}),
    onRejected ?? (() => {}),
  );
}

module.exports = { observePromise };
