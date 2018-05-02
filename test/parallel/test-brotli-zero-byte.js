// Flags: --expose-brotli
'use strict';
const common = require('../common');
const assert = require('assert');

const brotli = require('brotli');
const comp = new brotli.Compress();
const emptyBuffer = Buffer.alloc(0);
let received = 0;
comp.on('data', function(c) {
  received += c.length;
});

comp.on('end', common.mustCall(function() {
  assert.strictEqual(received, 1);
}));
comp.on('finish', common.mustCall());
comp.write(emptyBuffer);
comp.end();
