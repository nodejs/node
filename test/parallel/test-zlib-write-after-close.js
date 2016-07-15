'use strict';
const common = require('../common');
var assert = require('assert');
var zlib = require('zlib');

zlib.gzip('hello', common.mustCall(function(err, out) {
  var unzip = zlib.createGunzip();
  unzip.close(common.mustCall(function() {}));
  assert.throws(function() {
    unzip.write(out);
  });
}));
