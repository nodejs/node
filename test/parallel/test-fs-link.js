'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

// test creating and reading hard link
const srcPath = path.join(common.tmpDir, 'hardlink-target.txt');
const dstPath = path.join(common.tmpDir, 'link1.js');
const doesNotExist = path.join(common.tmpDir, '__this_should_not_exist');
const doesNotExistDst = path.join(common.tmpDir, '__this_should_not_exist_dst');
fs.writeFileSync(srcPath, 'hello world');

const callback = function(err) {
  assert.ifError(err);
  const dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.strictEqual('hello world', dstContent);
};

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

fs.link(doesNotExist, doesNotExistDst, common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.path, doesNotExist);
  assert.strictEqual(
    err.message,
    `ENOENT: no such file or directory, link '${doesNotExist}' ` +
    `-> '${doesNotExistDst}'`
  );
}));

fs.link(srcPath, srcPath, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EEXIST');
  assert.strictEqual(err.path, srcPath);
  assert.strictEqual(
    err.message,
    `EEXIST: file already exists, link '${srcPath}' -> '${srcPath}'`
  );
}));
