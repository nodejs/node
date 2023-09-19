'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

// Regression test for https://github.com/nodejs/node/issues/34606

const handler = {
  getOwnPropertyDescriptor: common.mustCallAtLeast(() => {
    return {};
  })
};

const proxy = new Proxy({}, handler);
assert.throws(() => vm.runInNewContext('p = 6', proxy),
              /getOwnPropertyDescriptor/);
