'use strict';
var common = require('../common');
var assert = require('assert');
var vm = require('vm');

console.error('before');

// undefined reference
vm.runInNewContext('foo.bar = 5;');

console.error('after');
