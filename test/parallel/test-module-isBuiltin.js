'use strict';
require('../common');
const assert = require('assert');
const { isBuiltin } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(isBuiltin('http'));
assert(isBuiltin('sys'));
assert(isBuiltin('node:fs'));
assert(isBuiltin('node:test'));

// Does not include internal or disabled experimental modules
assert(!isBuiltin('internal/errors'));
assert(!isBuiltin('node:bench'));
assert(!isBuiltin('node:bench/reporters'));
assert(!isBuiltin('bench'));
assert(!isBuiltin('bench/reporters'));
assert(!isBuiltin('test'));
assert(!isBuiltin(''));
assert(!isBuiltin(undefined));
