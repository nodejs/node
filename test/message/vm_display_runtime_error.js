'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

console.error('beginning');

vm.runInThisContext('throw new Error("boo!")', { filename: 'test.vm' });

console.error('end');
