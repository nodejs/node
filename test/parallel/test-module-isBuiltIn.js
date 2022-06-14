'use strict';
require('../common');
const assert = require('assert');
const { isBuiltIn } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(isBuiltIn('http'));
assert(isBuiltIn('sys'));

// Does not include internal modules
assert(!isBuiltIn('internal'));
assert(!isBuiltIn(''));
assert(!isBuiltIn(undefined));
