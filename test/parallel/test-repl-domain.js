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
var testMe = repl.start('', putIn);

putIn.write = function(data) {
  // Don't use assert for this because the domain might catch it, and
  // give a false negative.  Don't throw, just print and exit.
  if (data === 'OK\n') {
    console.log('ok');
  }
  else {
    console.error(data);
    process.exit(1);
  }
};

putIn.run([
  'require("domain").create().on("error", function() { console.log("OK") })'
  + '.run(function() { throw new Error("threw") })'
]);
