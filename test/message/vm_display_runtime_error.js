'use strict';
require('../common');
var vm = require('vm');

console.error('beginning');

vm.runInThisContext('throw new Error("boo!")', { filename: 'test.vm' });

console.error('end');
