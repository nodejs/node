'use strict';
const common = require('../common');

// This tests the creation of a vfs by monkey-patching fs and Module._stat.

const Module = require('module');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { join } = require('path');

const directory = tmpdir.resolve('directory');
const doesNotExist = tmpdir.resolve('does-not-exist');
const file = tmpdir.resolve('file.js');

tmpdir.refresh();
fs.writeFileSync(file, "module.exports = { a: 'b' }");
fs.mkdirSync(directory);

assert.strictEqual(Module._stat(directory), 1);
assert.ok(Module._stat(doesNotExist) < 0);
assert.strictEqual(Module._stat(file), 0);

const vfsDirectory = join(process.execPath, 'directory');
const vfsDoesNotExist = join(process.execPath, 'does-not-exist');
const vfsFile = join(process.execPath, 'file.js');

assert.ok(Module._stat(vfsDirectory) < 0);
assert.ok(Module._stat(vfsDoesNotExist) < 0);
assert.ok(Module._stat(vfsFile) < 0);

assert.deepStrictEqual(require(file), { a: 'b' });
assert.throws(() => require(vfsFile), { code: 'MODULE_NOT_FOUND' });

common.expectWarning(
  'ExperimentalWarning',
  'Module._stat is an experimental feature and might change at any time'
);

process.on('warning', common.mustCall());

const originalStat = Module._stat;
Module._stat = function(filename) {
  if (!filename.startsWith(process.execPath)) {
    return originalStat(filename);
  }

  if (filename === process.execPath) {
    return 1;
  }

  switch (filename) {
    case vfsDirectory:
      return 1;
    case vfsDoesNotExist:
      return -2;
    case vfsFile:
      return 0;
  }
};

const originalReadFileSync = fs.readFileSync;
// TODO(aduh95): We'd like to have a better way to achieve this without monkey-patching fs.
fs.readFileSync = function readFileSync(pathArgument, options) {
  if (!pathArgument.startsWith(process.execPath)) {
    return originalReadFileSync.apply(this, arguments);
  }
  if (pathArgument === vfsFile) {
    return "module.exports = { x: 'y' };";
  }
  throw new Error();
};

fs.realpathSync = function realpathSync(pathArgument, options) {
  return pathArgument;
};

assert.strictEqual(Module._stat(directory), 1);
assert.ok(Module._stat(doesNotExist) < 0);
assert.strictEqual(Module._stat(file), 0);

assert.strictEqual(Module._stat(vfsDirectory), 1);
assert.ok(Module._stat(vfsDoesNotExist) < 0);
assert.strictEqual(Module._stat(vfsFile), 0);

assert.strictEqual(Module._stat(process.execPath), 1);

assert.deepStrictEqual(require(file), { a: 'b' });
assert.deepStrictEqual(require(vfsFile), { x: 'y' });
