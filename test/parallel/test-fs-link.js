'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

// test creating and reading hard link
const srcPath = path.join(common.tmpDir, 'hardlink-target.txt');
const dstPath = path.join(common.tmpDir, 'link1.js');
fs.writeFileSync(srcPath, 'hello world');

function callback(err) {
  assert.ifError(err);
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual('hello world', dstContent);
}

fs.link(srcPath, dstPath, common.mustCall(callback));

// test error outputs

assert.throws(
  function() {
    fs.link();
  },
  /src must be a string or Buffer/
);

assert.throws(
  function() {
    fs.link('abc');
  },
  /dest must be a string or Buffer/
);
