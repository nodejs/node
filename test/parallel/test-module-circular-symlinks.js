'use strict';

// This tests to make sure that modules with symlinked circular dependencies
// do not blow out the module cache and recurse forever. See issue
// https://github.com/nodejs/node/pull/5950 for context. PR #5950 attempted
// to solve a problem with symlinked peer dependencies by caching using the
// symlink path. Unfortunately, that breaks the case tested in this module
// because each symlinked module, despite pointing to the same code on disk,
// is loaded and cached as a separate module instance, which blows up the
// cache and leads to a recursion bug.

// This test should pass in Node.js v4 and v5. It should pass in Node.js v6
// after https://github.com/nodejs/node/pull/5950 has been reverted.

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

// {tmpDir}
// ├── index.js
// └── node_modules
//     ├── moduleA
//     │   ├── index.js
//     │   └── node_modules
//     │       └── moduleB -> {tmpDir}/node_modules/moduleB
//     └── moduleB
//         ├── index.js
//         └── node_modules
//         └── moduleA -> {tmpDir}/node_modules/moduleA

common.refreshTmpDir();
const tmpDir = common.tmpDir;

const node_modules = path.join(tmpDir, 'node_modules');
const moduleA = path.join(node_modules, 'moduleA');
const moduleB = path.join(node_modules, 'moduleB');
const moduleA_link = path.join(moduleB, 'node_modules', 'moduleA');
const moduleB_link = path.join(moduleA, 'node_modules', 'moduleB');

fs.mkdirSync(node_modules);
fs.mkdirSync(moduleA);
fs.mkdirSync(moduleB);
fs.mkdirSync(path.join(moduleA, 'node_modules'));
fs.mkdirSync(path.join(moduleB, 'node_modules'));

try {
  fs.symlinkSync(moduleA, moduleA_link);
  fs.symlinkSync(moduleB, moduleB_link);
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}

fs.writeFileSync(path.join(tmpDir, 'index.js'),
                 'module.exports = require(\'moduleA\');', 'utf8');
fs.writeFileSync(path.join(moduleA, 'index.js'),
                 'module.exports = {b: require(\'moduleB\')};', 'utf8');
fs.writeFileSync(path.join(moduleB, 'index.js'),
                 'module.exports = {a: require(\'moduleA\')};', 'utf8');

// Ensure that the symlinks are not followed forever...
const obj = require(path.join(tmpDir, 'index'));
assert.ok(obj);
assert.ok(obj.b);
assert.ok(obj.b.a);
assert.ok(!obj.b.a.b);
