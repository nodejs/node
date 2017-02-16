'use strict';
require('../common');

// Flags: --expose-internals
//
// This test ensures that UTF-8 characters can be used in core JavaScript
// libraries built into Node's binary.

const assert = require('assert');
const character = require('internal/test/unicode');

assert.strictEqual(character, '✓');
