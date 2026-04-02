// Tests that after updating package.json exports to point to a different file,
// clearCache causes re-resolution to pick up the new export.
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache, createRequire } = require('node:module');

tmpdir.refresh();

// Create a temporary package with two entry points.
const pkgDir = path.join(tmpdir.path, 'node_modules', 'test-exports-pkg');
fs.mkdirSync(pkgDir, { recursive: true });

fs.writeFileSync(path.join(pkgDir, 'entry-a.js'),
                 'module.exports = "a";\n');
fs.writeFileSync(path.join(pkgDir, 'entry-b.js'),
                 'module.exports = "b";\n');

// Initial package.json: exports points to entry-a.js.
fs.writeFileSync(path.join(pkgDir, 'package.json'), JSON.stringify({
  name: 'test-exports-pkg',
  exports: './entry-a.js',
}));

// Create a require function rooted in tmpdir so it finds node_modules there.
const parentFile = path.join(tmpdir.path, 'parent.js');
fs.writeFileSync(parentFile, '');
const localRequire = createRequire(parentFile);

// First require — should resolve to entry-a.
const resultA = localRequire('test-exports-pkg');
assert.strictEqual(resultA, 'a');

// Update the package.json to point exports to entry-b.js.
fs.writeFileSync(path.join(pkgDir, 'package.json'), JSON.stringify({
  name: 'test-exports-pkg',
  exports: './entry-b.js',
}));

// Clear all caches for the package.
clearCache('test-exports-pkg', {
  parentURL: pathToFileURL(parentFile),
  resolver: 'require',
});

// Second require — should now resolve to entry-b.
const resultB = localRequire('test-exports-pkg');
assert.strictEqual(resultB, 'b');
