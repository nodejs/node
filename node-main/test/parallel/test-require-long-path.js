'use strict';
const common = require('../common');
if (!common.isWindows)
  common.skip('this test is Windows-specific.');

const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');

// Make a path that is more than 260 chars long.
const dirNameLen = Math.max(260 - tmpdir.path.length, 1);
const dirName = tmpdir.resolve('x'.repeat(dirNameLen));
const fullDirPath = path.resolve(dirName);

const indexFile = path.join(fullDirPath, 'index.js');
const otherFile = path.join(fullDirPath, 'other.js');

tmpdir.refresh();

fs.mkdirSync(fullDirPath);
fs.writeFileSync(indexFile, 'require("./other");');
fs.writeFileSync(otherFile, '');

require(indexFile);
require(otherFile);

tmpdir.refresh();
