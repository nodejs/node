'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { clearCache } = require('node:module');

const parentPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-parent.js');
const childPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-child.js');

const parent = require(parentPath);
assert.strictEqual(parent.child.count, 1);

const parentModule = require.cache[parentPath];
assert.ok(parentModule.children.some((child) => child.id === childPath));

const result = clearCache(childPath);
assert.strictEqual(result.require, true);
assert.strictEqual(result.import, false);

assert.strictEqual(require.cache[childPath], undefined);
assert.ok(!parentModule.children.some((child) => child.id === childPath));

const childReloaded = require(childPath);
assert.strictEqual(childReloaded.count, 2);

delete globalThis.__module_cache_cjs_child_counter;
