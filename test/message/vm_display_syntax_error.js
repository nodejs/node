'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

console.error('beginning');

vm.runInThisContext('var 5;', { filename: 'test.vm' });

console.error('end');
