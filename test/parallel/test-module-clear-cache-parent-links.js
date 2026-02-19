// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { clearCache } = require('node:module');
const { internalBinding } = require('internal/test/binding');

const {
  privateSymbols: {
    module_first_parent_private_symbol,
    module_last_parent_private_symbol,
  },
} = internalBinding('util');

const parentPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-parent.js');
const childPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-child.js');

require(parentPath);

const childModule = require.cache[childPath];
const parentModule = require.cache[parentPath];

assert.strictEqual(childModule[module_first_parent_private_symbol], parentModule);
assert.strictEqual(childModule[module_last_parent_private_symbol], parentModule);

clearCache(parentPath);

assert.strictEqual(childModule[module_first_parent_private_symbol], undefined);
assert.strictEqual(childModule[module_last_parent_private_symbol], undefined);

clearCache(childPath);
delete globalThis.__module_cache_cjs_child_counter;
