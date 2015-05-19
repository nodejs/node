'use strict';
var common = require('../common');
var assert = require('assert');

console.error('before');

// custom error throwing
throw ({ name: 'MyCustomError', message: 'This is a custom message' });

console.error('after');
