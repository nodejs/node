'use strict';
const common = require('../common');
const assert = require('assert');

console.error('before');

// custom error throwing
throw ({ foo: 'bar' });

console.error('after');
