'use strict';
require('../common');
const vm = require('vm');

console.error('beginning');

try {
  vm.runInThisContext('throw new Error("boo!")', { filename: 'test.vm'});
} catch (err) {
  console.error(err.stack);
}

vm.runInThisContext('throw new Error("spooky!")', { filename: 'test.vm' });

console.error('end');
