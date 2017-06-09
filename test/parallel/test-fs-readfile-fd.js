'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'empty.txt');

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
    assert.ifError(err);
    callback(fd, function() {
      fs.close(fd, function(err) {
        assert.ifError(err);
      });
    });
  });
}

function tempFdSync(callback) {
  const fd = fs.openSync(fn, 'r');
  callback(fd);
  fs.closeSync(fd);
}
