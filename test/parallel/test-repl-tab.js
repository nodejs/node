'use strict';
require('../common');
var assert = require('assert');
var util = require('util');
var repl = require('repl');
var zlib = require('zlib');

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
