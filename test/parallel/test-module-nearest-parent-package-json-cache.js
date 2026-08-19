'use strict';
// Flags: --expose-internals
// The nearest parent package.json lookup that every CommonJS module load
// performs is answered once per directory, not once per file.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { internalBinding } = require('internal/test/binding');
const packageJsonReader = require('internal/modules/package_json_reader');

tmpdir.refresh();
const root = tmpdir.resolve('pkg');
const sub = path.join(root, 'lib', 'sub');
fs.mkdirSync(sub, { recursive: true });
fs.writeFileSync(path.join(root, 'package.json'), JSON.stringify({ name: 'pkg', type: 'commonjs' }));
const files = [];
for (const dir of [path.join(root, 'lib'), sub]) {
  for (let i = 0; i < 5; i++) {
    const file = path.join(dir, `m${i}.js`);
    fs.writeFileSync(file, 'module.exports = __filename;');
    files.push(file);
  }
}

const modulesBinding = internalBinding('modules');
const original = modulesBinding.getNearestParentPackageJSON;
const calls = [];
modulesBinding.getNearestParentPackageJSON = common.mustCallAtLeast((checkPath) => {
  calls.push(checkPath);
  return original(checkPath);
}, 1);

for (const file of files) {
  assert.strictEqual(require(file), file);
}
// Ten modules in two directories: two lookups reach the binding.
assert.strictEqual(calls.length, 2, `binding called for: ${calls.join(', ')}`);

// Same answer (and the same object) for every file of a directory, and for
// the directory itself when asked with a trailing separator.
const viaFile = packageJsonReader.getNearestParentPackageJSON(files[0]);
assert.strictEqual(viaFile.data.name, 'pkg');
assert.strictEqual(packageJsonReader.getNearestParentPackageJSON(files[1]), viaFile);
assert.strictEqual(packageJsonReader.getNearestParentPackageJSON(path.join(root, 'lib') + path.sep), viaFile);
assert.strictEqual(calls.length, 2);

// A directory that has not been seen yet is looked up once more.
const other = path.join(root, 'other');
fs.mkdirSync(other);
fs.writeFileSync(path.join(other, 'x.js'), '');
assert.strictEqual(packageJsonReader.getNearestParentPackageJSON(path.join(other, 'x.js')).data.name, 'pkg');
assert.strictEqual(packageJsonReader.getNearestParentPackageJSON(path.join(other, 'y.js')).data.name, 'pkg');
assert.strictEqual(calls.length, 3);

modulesBinding.getNearestParentPackageJSON = original;
