'use strict';

const common = require('../common');

if (!common.isLinux) {
  common.skip('This test is only applicable to Linux');
}

const { ok, strictEqual } = require('assert');
const { join } = require('path');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const {
  cpSync,
  existsSync,
  mkdirSync,
  writeFileSync,
  readFileSync,
} = require('fs');
tmpdir.refresh();

const tmpdirPath = Buffer.from(join(tmpdir.path, 'a', 'c'));
const sepBuf = Buffer.from(path.sep);

mkdirSync(tmpdirPath, { recursive: true });

// The name is the Shift-JIS encoded version of こんにちは世界,
// or "Hello, World" in Japanese. On Linux systems, this name is
// a valid path name and should be handled correctly by the copy
// operation. However, the implementation of cp makes the assumption
// that the path names are UTF-8 encoded, so while we can create the
// file and check its existence using a Buffer, the recursive cp
// operation will fail because it tries to interpret every file name
// as UTF-8.
const name = Buffer.from([
  0x82, 0xB1, 0x82, 0xF1, 0x82, 0xC9, 0x82,
  0xBF, 0x82, 0xCD, 0x90, 0x6C, 0x8C, 0x8E,
]);
const testPath = Buffer.concat([tmpdirPath, sepBuf, name]);

writeFileSync(testPath, 'test content');
ok(existsSync(testPath));
strictEqual(readFileSync(testPath, 'utf8'), 'test content');

// The cpSync is expected to fail because the implementation does not
// properly handle non-UTF8 names in the path.

cpSync(join(tmpdir.path, 'a'), join(tmpdir.path, 'b'), {
  recursive: true,
  filter: common.mustCall(() => true, 1),
});
