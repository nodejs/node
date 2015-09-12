'use strict';
// Flags: --expose_internals

var assert = require('assert');

assert.equal(typeof require('internal/freelist').FreeList, 'function');
