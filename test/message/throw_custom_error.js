'use strict';
var common = require('../common');
var assert = require('assert');

// custom error throwing
throw ({ name: 'MyCustomError', message: 'This is a custom message' });
