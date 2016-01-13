'use strict';
require('../common');
var vm = require('vm');

console.error('beginning');

vm.runInThisContext('var 5;', { filename: 'test.vm' });

console.error('end');
