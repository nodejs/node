'use strict';
var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;
var path = require('path');

// `fs.readFile('/')` does not fail on FreeBSD, because you can open and read
// the directory there.
if (process.platform === 'freebsd') {
  console.error('Skipping test, platform not supported.');
  process.exit();
}

var callbacks = 0;

function test(env, cb) {
  var filename = path.join(common.fixturesDir, 'test-fs-readfile-error.js');
  var execPath = '"' + process.execPath + '" "' + filename + '"';
  var options = { env: env || {} };
  exec(execPath, options, function(err, stdout, stderr) {
    assert(err);
    assert.equal(stdout, '');
    assert.notEqual(stderr, '');
    cb('' + stderr);
  });
}

test({ NODE_DEBUG: '' }, function(data) {
  assert(/EISDIR/.test(data));
  assert(!/test-fs-readfile-error/.test(data));
  callbacks++;
});

test({ NODE_DEBUG: 'fs' }, function(data) {
  assert(/EISDIR/.test(data));
  assert(/test-fs-readfile-error/.test(data));
  callbacks++;
});

process.on('exit', function() {
  assert.equal(callbacks, 2);
});
