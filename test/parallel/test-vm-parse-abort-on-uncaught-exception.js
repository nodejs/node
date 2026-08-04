// Flags: --abort-on-uncaught-exception
'use strict';
require('../common');
const vm = require('vm');

// Regression test for https://github.com/nodejs/node/issues/13258

try {
  new vm.Script({ toString() { throw new Error('foo'); } }, {});
} catch {
  // Continue regardless of error.
}

try {
  new vm.Script('[', {});
} catch {
  // Continue regardless of error.
}
