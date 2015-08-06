'use strict';
const assert = require('assert');
const util = require('util');
const repl = require('repl');
const zlib = require('zlib');

// just use builtin stream inherited from Duplex
var putIn = zlib.createGzip();
var testMe = repl.start('', putIn, function(cmd, context, filename, callback) {
  callback(null, cmd);
});

testMe._domain.on('error', function(e) {
  assert.fail();
});

testMe.complete('', function(err, results) {
  assert.equal(err, null);
});
