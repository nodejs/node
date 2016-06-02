'use strict';
const common = require('../common');
const fs = require('fs');
const path = require('path');

if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
  return;
}

// make a path that is more than 260 chars long.
const dirNameLen = Math.max(260 - common.tmpDir.length, 1);
const dirName = path.join(common.tmpDir, 'x'.repeat(dirNameLen));
const fullDirPath = path.resolve(dirName);

const indexFile = path.join(fullDirPath, 'index.js');
const otherFile = path.join(fullDirPath, 'other.js');

common.refreshTmpDir();

fs.mkdirSync(fullDirPath);
fs.writeFileSync(indexFile, 'require("./other");');
fs.writeFileSync(otherFile, '');

require(indexFile);
require(otherFile);

common.refreshTmpDir();
