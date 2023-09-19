'use strict';
require('../common');
const assert = require('assert');
const { isBuiltin } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(isBuiltin('http'));
assert(isBuiltin('sys'));
assert(isBuiltin('node:fs'));
assert(isBuiltin('node:test'));

// Does not include internal modules
assert(!isBuiltin('internal/errors'));
assert(!isBuiltin('test'));
assert(!isBuiltin(''));
assert(!isBuiltin(undefined));
