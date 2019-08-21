'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const join = require('path').join;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Run this test on all platforms. While the 'm' flag is only available on
// Windows, it should be silently ignored on other platforms.

const filename = join(tmpdir.path, 'fmap.txt');
const text = 'Memory File Mapping Test';

fs.writeFileSync(filename, text, { flag: 'mw' });
const r1 = fs.readFileSync(filename, { encoding: 'utf8', flag: 'mr' });
assert.strictEqual(r1, text);

fs.writeFileSync(filename, text, { flag: 'mr+' });
const r2 = fs.readFileSync(filename, { encoding: 'utf8', flag: 'ma+' });
assert.strictEqual(r2, text);

fs.writeFileSync(filename, text, { flag: 'ma' });
const numericFlag = fs.constants.UV_FS_O_FILEMAP | fs.constants.O_RDONLY;
const r3 = fs.readFileSync(filename, { encoding: 'utf8', flag: numericFlag });
assert.strictEqual(r3, text + text);

const r4 = fs.readFileSync(filename, { encoding: 'utf8', flag: 'mw+' });
assert.strictEqual(r4, '');
