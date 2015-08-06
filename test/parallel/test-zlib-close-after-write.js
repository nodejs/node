'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

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
