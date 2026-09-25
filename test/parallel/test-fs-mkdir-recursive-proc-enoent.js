'use strict';

const common = require('../common');

if (!common.isLinux)
  common.skip('this regression is specific to procfs, which only exists on Linux');

// Regression test for https://github.com/nodejs/node/issues/66268.
//
// mkdir(path, { recursive: true }) walks up the path creating missing
// parent directories whenever mkdir() fails with ENOENT. procfs returns
// ENOENT for names it will never let you create even though the parent
// directory (/proc) already exists, which used to make the walk retry the
// same path forever instead of failing.

const assert = require('assert');
const fs = require('fs');

function unwritableProcPath() {
  return `/proc/node-test-mkdirp-${process.pid}-${Date.now()}`;
}

{
  const target = unwritableProcPath();
  assert.throws(() => {
    fs.mkdirSync(target, { recursive: true });
  }, { code: 'ENOENT' });
}

{
  const target = unwritableProcPath();
  fs.mkdir(target, { recursive: true }, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

{
  const target = unwritableProcPath();
  assert.rejects(
    fs.promises.mkdir(target, { recursive: true }),
    { code: 'ENOENT' },
  ).then(common.mustCall());
}
