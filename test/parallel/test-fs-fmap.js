'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const join = require('path').join;

const {
  O_CREAT = 0,
  O_RDONLY = 0,
  O_TRUNC = 0,
  O_WRONLY = 0,
  UV_FS_O_FILEMAP = 0
} = fs.constants;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Run this test on all platforms. While UV_FS_O_FILEMAP is only available on
// Windows, it should be silently ignored on other platforms.

const filename = join(tmpdir.path, 'fmap.txt');
const text = 'Memory File Mapping Test';

const mw = UV_FS_O_FILEMAP | O_TRUNC | O_CREAT | O_WRONLY;
const mr = UV_FS_O_FILEMAP | O_RDONLY;

fs.writeFileSync(filename, text, { flag: mw });
const r1 = fs.readFileSync(filename, { encoding: 'utf8', flag: mr });
assert.strictEqual(r1, text);
