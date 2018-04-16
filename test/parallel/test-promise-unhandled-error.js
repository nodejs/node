'use strict';

const common = require('../common');
const Countdown = require('../common/countdown');
const assert = require('assert');

process.env.NODE_UNHANDLED_REJECTION = 'ERROR';

// Verify that unhandled rejections always trigger uncaught exceptions instead
// of triggering unhandled rejections.

const err1 = new Error('One');
const err2 = new Error(
  'Unhandled promise rejection. This error originated either by throwing ' +
  'inside of an async function without a catch block, or by rejecting a ' +
  'promise which was not handled with .catch(). The promise rejected with the' +
  ' reason "null".'
);
err2.name = 'UnhandledPromiseRejection';

const errors = [err1, err2];
const identical = [true, false];

const ref = new Promise((res, rej) => {
  throw err1;
});
// Explicitly reject `null`.
Promise.reject(null);

process.on('warning', common.mustNotCall('warning'));
process.on('unhandledRejection', common.mustNotCall('unhandledRejection'));
process.on('rejectionHandled', common.mustNotCall('rejectionHandled'));

const timer = setTimeout(() => console.log(ref), 1000);

const counter = new Countdown(2, () => {
  clearTimeout(timer);
});

process.on('uncaughtException', common.mustCall((err, origin) => {
  counter.dec();
  assert.strictEqual(origin, 'FROM_PROMISE', err);
  const knownError = errors.shift();
  assert.deepStrictEqual(err, knownError);
  // Check if the errors are reference equal.
  assert(identical.shift() ? err === knownError : err !== knownError);
}, 2));
