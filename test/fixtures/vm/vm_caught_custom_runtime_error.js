'use strict';
require('../../common');
const vm = require('vm');

console.error('beginning');

// Regression test for https://github.com/nodejs/node/issues/7397:
// vm.runInThisContext() should not print out anything to stderr by itself.
try {
  vm.runInThisContext(`throw ({
    name: 'MyCustomError',
    message: 'This is a custom message'
  })`, { filename: 'test.vm' });
} catch (e) {
  console.error('received error', e.name);
}

console.error('end');
