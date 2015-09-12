'use strict';
var assert = require('assert');
var zlib = require('zlib');

var closed = false;

zlib.gzip('hello', function(err, out) {
  var unzip = zlib.createGunzip();
  unzip.write(out);
  unzip.close(function() {
    closed = true;
  });
});

process.on('exit', function() {
  assert(closed);
});
