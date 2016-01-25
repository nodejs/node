'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

// test creating and reading hard link
const srcPath = path.join(common.fixturesDir, 'cycles', 'root.js');
const srcContent = fs.readFileSync(srcPath, 'utf8');
const tmpSrcPath = path.join(common.tmpDir, 'root.js');
const dstPath = path.join(common.tmpDir, 'link1.js');

const callback = function(err) {
  if (err) throw err;
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual(srcContent, dstContent);
};

// copy source file to same directory to avoid cross-filesystem issues
fs.writeFileSync(tmpSrcPath, srcContent, 'utf8');

fs.link(srcPath, dstPath, common.mustCall(callback));

// test error outputs

assert.throws(
  function() {
    fs.link();
  },
  /src path/
);

assert.throws(
  function() {
    fs.link('abc');
  },
  /dest path/
);
