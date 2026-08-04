// Flags: --preserve-symlinks
'use strict';
// Refs: https://github.com/nodejs/node/pull/5950

// This test verifies that symlinked modules are able to find their peer
// dependencies when using the --preserve-symlinks command line flag.

// This test passes in v6.2+ with --preserve-symlinks on and in v6.0/v6.1.
// This test will fail in Node.js v4 and v5 and should not be backported.

const common = require('../common');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const tmpDir = tmpdir.path;

// Creates the following structure
// {tmpDir}
// ├── app
// │   ├── index.js
// │   └── node_modules
// │       ├── moduleA -> {tmpDir}/moduleA
// │       └── moduleB
// │           ├── index.js
// │           └── package.json
// └── moduleA
//     ├── index.js
//     └── package.json

const moduleA = path.join(tmpDir, 'moduleA');
const app = path.join(tmpDir, 'app');
const moduleB = path.join(app, 'node_modules', 'moduleB');
const moduleA_link = path.join(app, 'node_modules', 'moduleA');
fs.mkdirSync(moduleA);
fs.mkdirSync(app);
fs.mkdirSync(path.join(app, 'node_modules'));
fs.mkdirSync(moduleB);

// Attempt to make the symlink. If this fails due to lack of sufficient
// permissions, the test will bail out and be skipped.
try {
  fs.symlinkSync(moduleA, moduleA_link, 'dir');
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}

fs.writeFileSync(path.join(moduleA, 'package.json'),
                 JSON.stringify({ name: 'moduleA', main: 'index.js' }), 'utf8');
fs.writeFileSync(path.join(moduleA, 'index.js'),
                 'module.exports = require(\'moduleB\');', 'utf8');
fs.writeFileSync(path.join(app, 'index.js'),
                 '\'use strict\'; require(\'moduleA\');', 'utf8');
fs.writeFileSync(path.join(moduleB, 'package.json'),
                 JSON.stringify({ name: 'moduleB', main: 'index.js' }), 'utf8');
fs.writeFileSync(path.join(moduleB, 'index.js'),
                 'module.exports = 1;', 'utf8');

require(path.join(app, 'index')); // Should not throw.
