'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// test creating and reading hard link
const srcPath = path.join(tmpdir.path, 'hardlink-target.txt');
const dstPath = path.join(tmpdir.path, 'link1.js');
fs.writeFileSync(srcPath, 'hello world');

function callback(err) {
  assert.ifError(err);
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual(dstContent, 'hello world');
}

fs.link(srcPath, dstPath, common.mustCall(callback));

// test error outputs

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.link(i, '', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.link('', i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.linkSync(i, ''),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.linkSync('', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
