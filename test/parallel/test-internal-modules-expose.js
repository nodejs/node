'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');

assert.equal(typeof require('internal/freelist').FreeList, 'function');
