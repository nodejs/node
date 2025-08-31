'use strict';

const { mustCall, skip } = require('../common');
const { deepStrictEqual } = require('assert');
const {
  Worker,
  workerData,
  registerNotification,
  sendNotification,
  getNotifications,
  isMainThread,
  unregisterNotifications,
} = require('worker_threads');

if (!isMainThread && !workerData?.notification) {
  skip('This test only works on a main thread');
}

if (isMainThread) {
  const ownNotification = registerNotification(mustCall());

  const notification = registerNotification(mustCall(() => {
    worker.terminate();
  }));

  deepStrictEqual(getNotifications(), [ownNotification, notification]);
  sendNotification(ownNotification);

  const worker = new Worker(__filename, { workerData: { notification } });

  worker.on('exit', () => {
    unregisterNotifications();
  });
} else {
  sendNotification(workerData.notification);
}
