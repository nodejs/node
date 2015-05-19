'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var got_error = false,
    readdirDir = path.join(common.fixturesDir, 'readdir');

var files = ['are',
             'dir',
             'empty',
             'files',
             'for',
             'just',
             'testing.js',
             'these'];


console.log('readdirSync ' + readdirDir);
var f = fs.readdirSync(readdirDir);
console.dir(f);
assert.deepEqual(files, f.sort());


console.log('readdir ' + readdirDir);
fs.readdir(readdirDir, function(err, f) {
  if (err) {
    console.log('error');
    got_error = true;
  } else {
    console.dir(f);
    assert.deepEqual(files, f.sort());
  }
});

process.on('exit', function() {
  assert.equal(false, got_error);
  console.log('exit');
});


// readdir() on file should throw ENOTDIR
// https://github.com/joyent/node/issues/1869
(function() {
  var has_caught = false;

  try {
    fs.readdirSync(__filename);
  }
  catch (e) {
    has_caught = true;
    assert.equal(e.code, 'ENOTDIR');
  }

  assert(has_caught);
})();


(function() {
  var readdir_cb_called = false;

  fs.readdir(__filename, function(e) {
    readdir_cb_called = true;
    assert.equal(e.code, 'ENOTDIR');
  });

  process.on('exit', function() {
    assert(readdir_cb_called);
  });
})();
