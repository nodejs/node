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

common.expectsError(
  () => fs.linkSync(),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "src" argument must be one of type string, Buffer, or URL'
  }
);

common.expectsError(
  () => fs.linkSync('abc'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "dest" argument must be one of type string, Buffer, or URL'
  }
);

common.expectsError(
  () => fs.link(undefined, undefined, common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "src" argument must be one of type string, Buffer, or URL'
  }
);

common.expectsError(
  () => fs.link('abc', undefined, common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "dest" argument must be one of type string, Buffer, or URL'
  }
);
