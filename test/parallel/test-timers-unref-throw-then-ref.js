'use strict';
const common = require('../common');
const assert = require('assert');

process.once('uncaughtException', common.expectsError({
  message: 'Timeout Error'
}));

let called = false;
const t = setTimeout(() => {
  assert(!called);
  called = true;
  t.ref();
  throw new Error('Timeout Error');
}, 1).unref();

setTimeout(common.mustCall(), 1);
