'use strict';

const { mustCall, skip, platformTimeout, mustNotCall } = require('../common');
const Countdown = require('../common/countdown');
const { deepStrictEqual, strictEqual, throws } = require('assert');
const { Worker, workerData, isMainThread } = require('worker_threads');

if (!isMainThread && !workerData?.workerNotification) {
  skip('This test only works on a main thread');
}

if (isMainThread) {
  let worker;

  const countdown = new Countdown(3, mustCall(() => worker.terminate()));
  const ownNotification = process.registerNotification(mustCall());
  const workerNotification = process.registerNotification(() => countdown.dec());
  const temporaryNotification = process.registerNotification(mustNotCall());

  // Check ID management
  strictEqual(ownNotification, 1n);
  deepStrictEqual(process.getNotifications().sort(), [ownNotification, workerNotification, temporaryNotification]);

  // Immediately self notify
  process.notify(ownNotification);

  // Self notify in the workerNotification
  setTimeout(mustCall(() => {
    process.notify(workerNotification);
  }), platformTimeout(50));

  // Unregister the temporary notification after some time, this should lead the worker thread to fail
  setTimeout(mustCall(() => {
    process.unregisterNotification(temporaryNotification);
  }), platformTimeout(100));

  worker = new Worker(__filename, { workerData: { workerNotification, temporaryNotification } });
} else {
  // Immediately notify the main thread once
  process.notify(workerData.workerNotification);

  setTimeout(mustCall(() => {
    // This should fail as the main thread unregistered it
    throws(() => process.notify(workerData.temporaryNotification), { code: 'ERR_INVALID_NOTIFICATION' });

    // This will cause the main thread to kill this thread
    process.notify(workerData.workerNotification);
  }), platformTimeout(200));

  // Keep the worker alive until the main thread terminates it.
  setInterval(() => {}, 1e6);
}
