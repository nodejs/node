'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

{
  common.expectWarning(
    'DeprecationWarning',
    'In future versions of Node.js, fs.rmdir(path, { recursive: true }) ' +
      'will be removed. Use fs.rm(path, { recursive: true }) instead',
    'DEP0147'
  );
  const filePath = tmpdir.resolve('rmdir-recursive.txt');
  fs.writeFileSync(filePath, '');
  assert.throws(
    () => fs.rmdirSync(filePath, { recursive: true }),
    { code: common.isWindows ? 'ENOENT' : 'ENOTDIR' }
  );
}
