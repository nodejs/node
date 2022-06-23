'use strict';
require('../common');
const assert = require('assert');
const { isBuiltIn } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(isBuiltIn('http'));
assert(isBuiltIn('sys'));
assert(isBuiltIn('node:fs'));
assert(isBuiltIn('node:test'));

// Does not include internal modules
assert(!isBuiltIn('internal/errors'));
assert(!isBuiltIn('test'));
assert(!isBuiltIn(''));
assert(!isBuiltIn(undefined));
