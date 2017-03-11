'use strict';
require('../common');
const vm = require('vm');

console.error('before');

// undefined reference
vm.runInNewContext('foo.bar = 5;');

console.error('after');
