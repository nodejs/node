'use strict';
const common = require('../common');

if (!process.execArgv.includes('--abort-on-uncaught-exception'))
  common.requireFlags(['--abort-on-uncaught-exception']);

const vm = require('vm');

// Regression test for https://github.com/nodejs/node/issues/13258

try {
  new vm.Script({ toString() { throw new Error('foo'); } }, {});
} catch {}

try {
  new vm.Script('[', {});
} catch {}
