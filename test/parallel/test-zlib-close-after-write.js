'use strict';
const common = require('../common');
var zlib = require('zlib');

zlib.gzip('hello', common.mustCall(function(err, out) {
  var unzip = zlib.createGunzip();
  unzip.write(out);
  unzip.close(common.mustCall(function() {}));
}));
