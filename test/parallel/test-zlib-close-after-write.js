'use strict';
const common = require('../common');
const zlib = require('zlib');

zlib.gzip('hello', common.mustCall(function(err, out) {
  const unzip = zlib.createGunzip();
  unzip.write(out);
  unzip.close(common.mustCall(function() {}));
}));
