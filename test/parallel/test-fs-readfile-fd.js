'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs'),
    fn = path.join(common.fixturesDir, 'empty.txt');

tempFd(function(fd, close) {
  fs.readFile(fd, function(err, data) {
    assert.ok(data);
    close();
  });
});

tempFd(function(fd, close) {
  fs.readFile(fd, 'utf8', function(err, data) {
    assert.strictEqual('', data);
    close();
  });
});

tempFdSync(function(fd) {
  assert.ok(fs.readFileSync(fd));
});

tempFdSync(function(fd) {
  assert.strictEqual('', fs.readFileSync(fd, 'utf8'));
});

function tempFd(callback) {
  fs.open(fn, 'r', function(err, fd) {
    if (err) throw err;

    callback(fd, function() {
      fs.close(fd, function(err) {
        if (err) throw err;
      });
    });
  });
}

function tempFdSync(callback) {
  var fd = fs.openSync(fn, 'r');
  callback(fd);
  fs.closeSync(fd);
}
