'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const parentPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-parent.js');
const childPath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-child.js');

const parent = require(parentPath);
assert.strictEqual(parent.child.count, 1);

const parentModule = require.cache[parentPath];
assert.ok(parentModule.children.some((child) => child.id === childPath));

clearCache(childPath, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});

assert.strictEqual(require.cache[childPath], undefined);
assert.ok(!parentModule.children.some((child) => child.id === childPath));

const childReloaded = require(childPath);
assert.strictEqual(childReloaded.count, 2);

delete globalThis.__module_cache_cjs_child_counter;
