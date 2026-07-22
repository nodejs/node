'use strict';
const common = require('../common');

// This tests the creation of a vfs by monkey-patching fs and Module._stat.

const Module = require('module');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { deepStrictEqual, ok, strictEqual, throws } = require('assert');
const { join } = require('path');

const directory = tmpdir.resolve('directory');
const doesNotExist = tmpdir.resolve('does-not-exist');
const file = tmpdir.resolve('file.js');

tmpdir.refresh();
fs.writeFileSync(file, "module.exports = { a: 'b' }");
fs.mkdirSync(directory);

strictEqual(Module._stat(directory), 1);
ok(Module._stat(doesNotExist) < 0);
strictEqual(Module._stat(file), 0);

const vfsDirectory = join(process.execPath, 'directory');
const vfsDoesNotExist = join(process.execPath, 'does-not-exist');
const vfsFile = join(process.execPath, 'file.js');

ok(Module._stat(vfsDirectory) < 0);
ok(Module._stat(vfsDoesNotExist) < 0);
ok(Module._stat(vfsFile) < 0);

deepStrictEqual(require(file), { a: 'b' });
throws(() => require(vfsFile), { code: 'MODULE_NOT_FOUND' });

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

strictEqual(Module._stat(directory), 1);
ok(Module._stat(doesNotExist) < 0);
strictEqual(Module._stat(file), 0);

strictEqual(Module._stat(vfsDirectory), 1);
ok(Module._stat(vfsDoesNotExist) < 0);
strictEqual(Module._stat(vfsFile), 0);

strictEqual(Module._stat(process.execPath), 1);

deepStrictEqual(require(file), { a: 'b' });
deepStrictEqual(require(vfsFile), { x: 'y' });
