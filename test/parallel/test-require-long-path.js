'use strict';
const common = require('../common');
const fs = require('fs');
const path = require('path');
const os = require('os');

var tmpDir = common.tmpDir;
var useOsTmpDir = false;

// use real OS tmp dir on Linux when it is readable/writable
// to avoid failing tests on ecryptfs filesystems:
// https://github.com/nodejs/node/issues/2255
if (os.type() == 'Linux') {
  try {
    fs.accessSync(os.tmpdir(), fs.R_OK | fs.W_OK);
    useOsTmpDir = true;
    tmpDir = path.join(os.tmpdir(),
      'node-' + process.version + '-test-' + (Math.random() * 1e6 | 0));
    fs.mkdirSync(tmpDir);
  } catch (e) {
    useOsTmpDir = false;
    tmpDir = common.tmpDir;
  }
}

// make a path that is more than 260 chars long.
const dirNameLen = Math.max(260 - tmpDir.length, 1);
const dirName = path.join(tmpDir, 'x'.repeat(dirNameLen));
const fullDirPath = path.resolve(dirName);

const indexFile = path.join(fullDirPath, 'index.js');
const otherFile = path.join(fullDirPath, 'other.js');

if (!useOsTmpDir) {
  common.refreshTmpDir();
}

fs.mkdirSync(fullDirPath);
fs.writeFileSync(indexFile, 'require("./other");');
fs.writeFileSync(otherFile, '');

require(indexFile);
require(otherFile);

if (useOsTmpDir) {
  fs.unlinkSync(indexFile);
  fs.unlinkSync(otherFile);
  fs.rmdirSync(fullDirPath);
  fs.rmdirSync(tmpDir);
} else {
  common.refreshTmpDir();
}
