'use strict';
require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const tmp = tmpdir.path;

tmpdir.refresh();

const filename = path.resolve(tmp, 'truncate-sync-file.txt');

fs.writeFileSync(filename, 'hello world', 'utf8');

const fd = fs.openSync(filename, 'r+');

fs.truncateSync(fd, 5);
assert(fs.readFileSync(fd).equals(Buffer.from('hello')));

fs.closeSync(fd);
fs.unlinkSync(filename);
