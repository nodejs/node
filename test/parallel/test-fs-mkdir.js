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

(function() {
  var ncalls = 0;
  var pathname = common.tmpDir + '/test1';

  unlink(pathname);

  fs.mkdir(pathname, function(err) {
    assert.equal(err, null);
    assert.equal(common.fileExists(pathname), true);
    ncalls++;
  });

  process.on('exit', function() {
    unlink(pathname);
    assert.equal(ncalls, 1);
  });
})();

(function() {
  var ncalls = 0;
  var pathname = common.tmpDir + '/test2';

  unlink(pathname);

  fs.mkdir(pathname, 511 /*=0777*/, function(err) {
    assert.equal(err, null);
    assert.equal(common.fileExists(pathname), true);
    ncalls++;
  });

  process.on('exit', function() {
    unlink(pathname);
    assert.equal(ncalls, 1);
  });
})();

(function() {
  var pathname = common.tmpDir + '/test3';

  unlink(pathname);
  fs.mkdirSync(pathname);

  var exists = common.fileExists(pathname);
  unlink(pathname);

  assert.equal(exists, true);
})();

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(function() {});
