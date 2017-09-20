'use strict';

const common = require('../common');
if (!common.isLinux)
  common.skip('Test is linux specific.');

const path = require('path');
const fs = require('fs');
const assert = require('assert');

common.refreshTmpDir();
const filename = '\uD83D\uDC04';
const root = Buffer.from(`${common.tmpDir}${path.sep}`);
const filebuff = Buffer.from(filename, 'ucs2');
const fullpath = Buffer.concat([root, filebuff]);

try {
  fs.closeSync(fs.openSync(fullpath, 'w+'));
} catch (e) {
  if (e.code === 'EINVAL')
    common.skip('test requires filesystem that supports UCS2');
  throw e;
}

fs.readdir(common.tmpDir, 'ucs2', common.mustCall((err, list) => {
  assert.ifError(err);
  assert.strictEqual(1, list.length);
  const fn = list[0];
  assert.deepStrictEqual(filebuff, Buffer.from(fn, 'ucs2'));
  assert.strictEqual(fn, filename);
}));
