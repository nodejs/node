var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var openFd;
fs.open(__filename, 'r', function(err, fd) {
  if (err) {
    throw err;
  }

  openFd = fd;
});

process.addListener('exit', function() {
  assert.ok(openFd);
});

