'use strict';
// Flags: --no-deprecation
const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');

// Calling a promisified function that returns a Promise emits a regular
// warning, once per promisified function. It is not a deprecation warning, so
// --no-deprecation does not affect it; it can be silenced with
// --disable-warning=PROMISIFY_RETURNED_PROMISE.
process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'Warning');
  assert.strictEqual(warning.code, 'PROMISIFY_RETURNED_PROMISE');
  assert.strictEqual(
    warning.message,
    'Calling promisify on a function that returns a Promise is likely a mistake.');
}, 5));

// The callback is still used to settle the result, and the warning is only
// emitted once even when the promisified function is called several times.
{
  const fn = promisify(async (value, callback) => { callback(null, value); });
  fn(1).then((value) => assert.strictEqual(value, 1)).then(common.mustCall());
  fn(2).then((value) => assert.strictEqual(value, 2)).then(common.mustCall());
}

// Errors passed to the callback still reject the result.
{
  const error = new Error('callback error');
  assert.rejects(promisify(async (callback) => { callback(error); })(), error)
    .then(common.mustCall());
}

// Rejections of the returned Promise are forwarded instead of being left
// unhandled.
{
  const error = new Error('returned promise rejection');
  assert.rejects(promisify(async () => { throw error; })(), error)
    .then(common.mustCall());
}

// If the callback already settled the result, a later rejection of the
// returned Promise is still reported as an unhandled rejection rather than
// being silently swallowed.
{
  const error = new Error('rejection after callback');
  process.on('unhandledRejection', common.mustCall((reason) => {
    assert.strictEqual(reason, error);
  }));
  promisify(async (callback) => {
    callback(null, 'value');
    throw error;
  })().then((value) => assert.strictEqual(value, 'value')).then(common.mustCall());
}

// A returned Promise that fulfills without the callback being called does not
// settle the result.
{
  promisify(async () => {})().then(common.mustNotCall('never settling promise expected'));
}
