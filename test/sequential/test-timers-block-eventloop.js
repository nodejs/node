// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { sleep } = require('internal/util');

let called = false;
const t1 = setInterval(() => {
  assert(!called);
  called = true;
  setImmediate(common.mustCall(() => {
    clearInterval(t1);
    clearInterval(t2);
  }));
}, 10);

const t2 = setInterval(() => {
  sleep(20);
}, 10);
