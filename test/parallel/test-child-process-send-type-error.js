'use strict';
require('../common');
const assert = require('assert');
const cp = require('child_process');

function noop() {}

function fail(proc, args) {
  assert.throws(() => {
    proc.send.apply(proc, args);
  }, /"options" argument must be an object/);
}

let target = process;

if (process.argv[2] !== 'child')
  target = cp.fork(__filename, ['child']);

fail(target, ['msg', null, null]);
fail(target, ['msg', null, '']);
fail(target, ['msg', null, 'foo']);
fail(target, ['msg', null, 0]);
fail(target, ['msg', null, NaN]);
fail(target, ['msg', null, 1]);
fail(target, ['msg', null, null, noop]);
