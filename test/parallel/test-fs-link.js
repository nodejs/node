'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

{
  // Loading tmpdir module here because asynchronous nature of tests means that
  // it will be subject to race conditions if used elsewhere in the test file.
  const tmpdir = require('../common/tmpdir');
  // test creating and reading hard link
  const srcPath = path.join(tmpdir.path, 'hardlink-target.txt');
  const dstPath = path.join(tmpdir.path, 'link1.js');

  function callback(err) {
    assert.ifError(err);
    const dstContent = fs.readFileSync(dstPath, 'utf8');
    assert.strictEqual(dstContent, 'hello world');
  }

  const args = [srcPath, dstPath, callback];

  const setup = () => {
    tmpdir.refresh();
    fs.writeFileSync(srcPath, 'hello world');
  };

  common.fsTest('link', args, { setup });
}

// test error outputs

const expectedError = (e) => {
  assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
  assert.ok(e instanceof TypeError);
};

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.fsTest('link', [i, '', expectedError], { throws: true });
  common.fsTest('link', ['', i, expectedError], { throws: true });
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
