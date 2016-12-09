'use strict';

const common = require('../common');
const path = require('path');
const fs = require('fs');
const assert = require('assert');

if (!common.isLinux) {
  common.skip('Test is linux specific.');
  return;
}

common.refreshTmpDir();
const filename = '\uD83D\uDC04';
const root = Buffer.from(`${common.tmpDir}${path.sep}`);
const filebuff = Buffer.from(filename, 'ucs2');
const fullpath = Buffer.concat([root, filebuff]);

fs.closeSync(fs.openSync(fullpath, 'w+'));

fs.readdir(common.tmpDir, 'ucs2', (err, list) => {
  if (err) throw err;
  assert.equal(1, list.length);
  const fn = list[0];
  assert.deepStrictEqual(filebuff, Buffer.from(fn, 'ucs2'));
  assert.strictEqual(fn, filename);
});

process.on('exit', () => {
  fs.unlinkSync(fullpath);
});
