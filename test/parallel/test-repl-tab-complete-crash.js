'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const repl = require('repl');

var referenceErrorCount = 0;

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    const self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\n');
    });
  };
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function(msg) {
  if (msg.startsWith('ReferenceError: ')) {
    referenceErrorCount++;
  }
};

const putIn = new ArrayStream();
const testMe = repl.start('', putIn);

// https://github.com/nodejs/node/issues/3346
// Tab-completion for an undefined variable inside a function should report a
// ReferenceError.
putIn.run(['.clear']);
putIn.run(['function () {']);
testMe.complete('arguments.');

process.on('exit', function() {
  assert.strictEqual(referenceErrorCount, 1);
});
