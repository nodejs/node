var assert = require('assert');
var util = require('util');
var repl = require('./index');

function ArrayStream() {}
// A stream to push an array into a REPL
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

var putIn = new ArrayStream();
var testMe = repl.start('', putIn, function(cmd, context, filename, callback) {
  callback(null, cmd);
});

testMe._domain.on('error', function (e) {
  assert.fail();
});

testMe.complete('', function () {});
