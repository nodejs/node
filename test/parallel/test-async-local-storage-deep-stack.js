'use strict';
const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');

// Regression test for: https://github.com/nodejs/node/issues/34556

const also = new AsyncLocalStorage();

const done = common.mustCall();

function run(count) {
  if (count !== 0) return also.run({}, run, --count);
  done();
}
run(1000);
