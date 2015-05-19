'use strict';
var common = require('../common');
var assert = require('assert');

console.error('before');

// custom error throwing
throw ({ foo: 'bar' });

console.error('after');
