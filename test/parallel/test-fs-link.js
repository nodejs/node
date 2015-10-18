'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

// test creating and reading hard link
const srcPath = path.join(common.fixturesDir, 'cycles', 'root.js');
const dstPath = path.join(common.tmpDir, 'link1.js');

const callback = function(err) {
  if (err) throw err;
  const srcContent = fs.readFileSync(srcPath, 'utf8');
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual(srcContent, dstContent);
};

fs.link(srcPath, dstPath, common.mustCall(callback));
