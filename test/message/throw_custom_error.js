'use strict';
const common = require('../common');
const assert = require('assert');

console.error('before');

// custom error throwing
throw ({ name: 'MyCustomError', message: 'This is a custom message' });

console.error('after');
