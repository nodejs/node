'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var dirname = path.dirname(__filename);
var d = path.join(common.tmpDir, 'dir');

var mkdir_error = false;
var rmdir_error = false;

fs.mkdir(d, 0o666, function(err) {
  if (err) {
    console.log('mkdir error: ' + err.message);
    mkdir_error = true;
  } else {
    fs.mkdir(d, 0o666, function(err) {
      console.log('expect EEXIST error: ', err);
      assert.ok(err.message.match(/^EEXIST/), 'got EEXIST message');
      assert.equal(err.code, 'EEXIST', 'got EEXIST code');
      assert.equal(err.path, d, 'got proper path for EEXIST');

      console.log('mkdir okay!');
      fs.rmdir(d, function(err) {
        if (err) {
          console.log('rmdir error: ' + err.message);
          rmdir_error = true;
        } else {
          console.log('rmdir okay!');
        }
      });
    });
  }
});

process.on('exit', function() {
  assert.equal(false, mkdir_error);
  assert.equal(false, rmdir_error);
  console.log('exit');
});
