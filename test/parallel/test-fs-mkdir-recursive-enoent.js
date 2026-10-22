'use strict';
// Refs: https://github.com/nodejs/node/issues/66268
// Refs: https://github.com/nodejs/node/issues/66890
// On procfs, mkdir fails with ENOENT although the parent exists. A recursive
// mkdir must report the error instead of retrying forever — including when
// the failing path is nested under another missing intermediate component.
const common = require('../common');
const fs = require('fs');

if (!common.isLinux) common.skip('procfs is Linux only');

const assert = require('assert');
const expected = { code: 'ENOENT', syscall: 'mkdir' };

for (const dir of [
  `/proc/node-test-${process.pid}`,
  `/proc/node-test-${process.pid}/deeper`,
]) {
  assert.throws(() => fs.mkdirSync(dir, { recursive: true }), expected);

  fs.mkdir(dir, { recursive: true }, common.mustCall((err) => {
    assert.strictEqual(err.code, expected.code);
    assert.strictEqual(err.syscall, expected.syscall);
  }));

  assert.rejects(fs.promises.mkdir(dir, { recursive: true }), expected)
    .then(common.mustCall());
}
