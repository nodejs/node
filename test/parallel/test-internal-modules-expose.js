'use strict';
// Flags: --expose_internals

var common = require('../common');
var assert = require('assert');

assert.equal(typeof require('internal/freelist').FreeList, 'function');
