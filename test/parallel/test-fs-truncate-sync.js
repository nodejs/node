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

fs.truncateSync(filename, 5);
assert(fs.readFileSync(filename).equals(Buffer.from('hello')));

fs.unlinkSync(filename);
