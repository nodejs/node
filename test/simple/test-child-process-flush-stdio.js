'use strict';
var cp = require('child_process');
var common = require('../common');
var assert = require('assert');

var buffer = [];
var p = cp.spawn('echo', ['123']);
p.on('close', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(Buffer.concat(buffer).toString().trim(), '123');
}));
p.stdout.on('readable', function() {
  var buf;
  while (buf = this.read())
    buffer.push(buf);
});
