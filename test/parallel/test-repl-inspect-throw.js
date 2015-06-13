'use strict';
var assert = require('assert');
var common = require('../common');
var util   = require('util');
var repl   = require('repl');

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\n');
    });
  };
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

var putIn = new ArrayStream();
var testMe = repl.start({
  input: putIn,
  output: putIn,
  terminal: true
});

// Regression test: an exception raised during the 'keypress' event in readline
// would corrupt the state of the escape sequence decoder, rendering the REPL
// (and possibly your terminal) completely unusable.
//
// We can trigger this by triggering a throw inside util.inspect, which is used
// to print a string representation of what you evaluate in the REPL.
testMe.context.throwObj = {
  inspect: function() {
    throw new Error('inspect error');
  }
};
testMe.context.reached = false;

// Exceptions for whatever reason do not reach the normal error handler after
// the inspect throw, but will work if run in another tick.
process.nextTick(function() {
  assert(testMe.context.reached);
});

putIn.run([
  'throwObj',
  'reached = true'
]);
