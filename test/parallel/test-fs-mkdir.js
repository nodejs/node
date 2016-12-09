'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

function unlink(pathname) {
  try {
    fs.rmdirSync(pathname);
  } catch (e) {
  }
}

common.refreshTmpDir();

{
  const pathname = common.tmpDir + '/test1';

  unlink(pathname);

  fs.mkdir(pathname, common.mustCall(function(err) {
    assert.strictEqual(err, null);
    assert.strictEqual(common.fileExists(pathname), true);
  }));

  process.on('exit', function() {
    unlink(pathname);
  });
}

{
  const pathname = common.tmpDir + '/test2';

  unlink(pathname);

  fs.mkdir(pathname, 0o777, common.mustCall(function(err) {
    assert.equal(err, null);
    assert.equal(common.fileExists(pathname), true);
  }));

  process.on('exit', function() {
    unlink(pathname);
  });
}

{
  const pathname = common.tmpDir + '/test3';

  unlink(pathname);
  fs.mkdirSync(pathname);

  const exists = common.fileExists(pathname);
  unlink(pathname);

  assert.strictEqual(exists, true);
}

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(function() {});
