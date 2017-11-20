'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

common.refreshTmpDir();

{
  const pathname = `${common.tmpDir}/test1`;

  fs.mkdir(pathname, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(common.fileExists(pathname), true);
  }));
}

{
  const pathname = `${common.tmpDir}/test2`;

  fs.mkdir(pathname, 0o777, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(common.fileExists(pathname), true);
  }));
}

{
  const pathname = `${common.tmpDir}/test3`;

  fs.mkdirSync(pathname);

  const exists = common.fileExists(pathname);
  assert.strictEqual(exists, true);
}

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(() => {});
