'use strict';
const common = require('../common');
const assert = require('assert');

const zlib = require('zlib');
const gz = zlib.Gzip();
const emptyBuffer = Buffer.alloc(0);
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
