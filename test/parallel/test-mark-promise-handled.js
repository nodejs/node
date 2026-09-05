'use strict';

const common = require('../common');
const assert = require('node:assert');
const { markPromiseAsHandled } = require('node:util');

process.on('unhandledrejection', common.mustNotCall());

markPromiseAsHandled(Promise.reject(123));

{
  const { promise, reject } = Promise.withResolvers();
  markPromiseAsHandled(promise);
  reject(123);
}

{
  const { promise, reject } = Promise.withResolvers();
  reject(123);
  markPromiseAsHandled(promise);
}

assert.throws(() => markPromiseAsHandled(123), {
  code: 'ERR_INVALID_ARG_TYPE',
});
