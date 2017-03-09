'use strict';
require('../common');
const vm = require('vm');

console.error('beginning');

vm.runInThisContext('throw new Error("boo!")', { filename: 'test.vm' });

console.error('end');
