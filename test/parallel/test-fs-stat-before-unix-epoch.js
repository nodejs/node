// Check that file timestamps from before the UNIX epoch
// are set and retrieved correctly.

'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const date = new Date('1969-07-20 02:56:00Z');
const filename = path.join(tmpdir.path, '42.txt');

tmpdir.refresh();
fs.writeFileSync(filename, '42');
fs.utimesSync(filename, date, date);
const s = fs.statSync(filename);
assert.deepStrictEqual(s.atime, date);
assert.deepStrictEqual(s.mtime, date);
