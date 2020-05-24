'use strict';
const assert = require('assert');

// Accessing module.parent should not be falsy.
assert.ok(module.parent)
