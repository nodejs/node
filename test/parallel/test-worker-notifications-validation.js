'use strict';

const { mustCall } = require('../common');
const { throws } = require('assert');
const {
  registerNotification,
  sendNotification,
  unregisterNotification,
  Worker,
  parentPort,
  workerData,
} = require('worker_threads');

// Generic argument validation
{
  throws(() => registerNotification(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => registerNotification(123), { code: 'ERR_INVALID_ARG_TYPE' });

  throws(() => unregisterNotification(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => unregisterNotification('STRING'), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => unregisterNotification(-123), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => unregisterNotification(123), { code: 'ERR_INVALID_NOTIFICATION' });

  throws(() => sendNotification(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => sendNotification('STRING'), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => sendNotification(-123), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => sendNotification(123), { code: 'ERR_INVALID_NOTIFICATION' });
}

// Unregistering a notification from another thread should not be permitted
{
  // The initial thread spawns another thread which registers a notification handler
  if (!workerData?.registerNotification) {
    const worker = new Worker(__filename, { workerData: { registerNotification: true } });

    worker.once('message', mustCall((arg) => {
      throws(() => unregisterNotification(arg), { code: 'ERR_INVALID_NOTIFICATION' });
      worker.terminate();
    }));
  } else {
    const notification = registerNotification(() => {});
    parentPort.postMessage(notification);
  }
}
