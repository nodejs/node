'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const zlib = require('zlib');

// just use builtin stream inherited from Duplex
var putIn = zlib.createGzip();
var testMe = repl.start('', putIn, function(cmd, context, filename, callback) {
  callback(null, cmd);
});

testMe._domain.on('error', common.fail);

testMe.complete('', function(err, results) {
  assert.equal(err, null);
});
