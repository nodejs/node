'use strict';
var common = require('../common');
var assert = require('assert');
var vm = require('vm');

console.error('beginning');

vm.runInThisContext('throw new Error("boo!")', { filename: 'test.vm' });

console.error('end');
