'use strict';
const common = require('../common');
var assert = require('assert');

var zlib = require('zlib');
var gz = zlib.Gzip();
var emptyBuffer = Buffer.alloc(0);
var received = 0;
gz.on('data', function(c) {
  received += c.length;
});

gz.on('end', common.mustCall(function() {
  assert.strictEqual(received, 20);
}));
gz.on('finish', common.mustCall(function() {}));
gz.write(emptyBuffer);
gz.end();
