'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test creating and reading hard link
const srcPath = tmpdir.resolve('hardlink-target.txt');
const dstPath = tmpdir.resolve('link1.js');
fs.writeFileSync(srcPath, 'hello world');

function callback(err) {
  assert.ifError(err);
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual(dstContent, 'hello world');
}

fs.link(srcPath, dstPath, common.mustCall(callback));

// test error outputs

[false, 1, [], {}, null, undefined].forEach((i) => {
  assert.throws(
    () => fs.link(i, '', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.link('', i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.linkSync(i, ''),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.linkSync('', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});
