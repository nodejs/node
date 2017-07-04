'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

if (!common.isWindows) {
  common.skip('This test is for Windows only.');
  return;
}

common.refreshTmpDir();

const DATA_VALUE = 'hello';

// Refs: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
// Ignore '/', '\\' and ':'
const REVERSED_CHARACTERS = '<>"|?*';

Array.prototype.forEach.call(REVERSED_CHARACTERS, (ch) => {
  const pathname = path.join(common.tmpDir, `somefile_${ch}`);
  assert.throws(
    () => {
      fs.writeFileSync(pathname, DATA_VALUE);
    },
    /^Error: ENOENT: no such file or directory, open '.*'$/,
    `failed with '${ch}'`);
});

// Test for ':' (NTFS data streams).
// Refs: https://msdn.microsoft.com/en-us/library/windows/desktop/bb540537.aspx
const pathname = path.join(common.tmpDir, 'foo:bar');
fs.writeFileSync(pathname, DATA_VALUE);
const fileDataStream = fs.createReadStream(pathname, {
  encoding: 'utf8'
});
fileDataStream.on('data', common.mustCall((data) => {
  assert.strictEqual(data.toString(), DATA_VALUE);
}));
