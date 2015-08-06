'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

console.error('before');

// undefined reference
vm.runInNewContext('foo.bar = 5;');

console.error('after');
