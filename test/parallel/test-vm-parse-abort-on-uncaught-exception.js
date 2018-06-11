'use strict';
require('../common');
const vm = require('vm');

// Regression test for https://github.com/nodejs/node/issues/13258

try {
  new vm.Script({ toString() { throw new Error('foo'); } }, {});
  new vm.Script('[', {});
} catch (err) {}
