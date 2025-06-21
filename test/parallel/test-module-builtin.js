'use strict';
require('../common');
const assert = require('assert');
const { builtinModules } = require('module');

// Includes modules in lib/ (even deprecated ones)
assert(builtinModules.includes('http'));
assert(builtinModules.includes('sys'));

// Does not include internal modules
assert.deepStrictEqual(
  builtinModules.filter((mod) => mod.startsWith('internal/')),
  []
);
