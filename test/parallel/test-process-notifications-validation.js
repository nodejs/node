'use strict';

const { mustCall } = require('../common');
const { throws } = require('assert');
const { Worker, parentPort, workerData } = require('worker_threads');

// Generic argument validation
{

  throws(() => process.registerNotification(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => process.registerNotification(123), { code: 'ERR_INVALID_ARG_TYPE' });

  throws(() => process.unregisterNotification(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => process.unregisterNotification('STRING'), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => process.unregisterNotification(123.45), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => process.unregisterNotification(-123), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => process.unregisterNotification(123), { code: 'ERR_INVALID_NOTIFICATION' });
  throws(() => process.unregisterNotification(
    BigInt(Number.MAX_SAFE_INTEGER + 1000)), { code: 'ERR_INVALID_NOTIFICATION' },
  );

  throws(() => process.notify(), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => process.notify('STRING'), { code: 'ERR_INVALID_ARG_TYPE' });
  throws(() => process.notify(123.45), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => process.notify(-123), { code: 'ERR_OUT_OF_RANGE' });
  throws(() => process.notify(123), { code: 'ERR_INVALID_NOTIFICATION' });
  throws(() => process.notify(BigInt(Number.MAX_SAFE_INTEGER + 1000)), { code: 'ERR_INVALID_NOTIFICATION' });
}

// Unregistering a notification from another thread should not be permitted
{
  // The initial thread spawns another thread which registers a notification handler
  if (!workerData?.registerNotification) {
    const worker = new Worker(__filename, { workerData: { registerNotification: true } });

    worker.once('message', mustCall((arg) => {
      throws(() => process.unregisterNotification(arg), { code: 'ERR_INVALID_NOTIFICATION' });
      worker.terminate();
    }));
  } else {
    const notification = process.registerNotification(() => {});
    parentPort.postMessage(notification);
  }
}
