'use strict';
var common = require('../common');
var assert = require('assert');
var vm = require('vm');

console.error('beginning');

vm.runInThisContext('var 5;', { filename: 'test.vm' });

console.error('end');
