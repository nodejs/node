'use strict';
const common = require('../common');
const assert = require('assert');
const timers = require('timers');
const { promisify } = require('util');

/* eslint-disable no-restricted-syntax */

common.crashOnUnhandledRejection();

const setTimeout = promisify(timers.setTimeout);
const setImmediate = promisify(timers.setImmediate);

{
  const promise = setTimeout(1);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setTimeout(1, 'foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const promise = setImmediate();
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setImmediate('foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}
