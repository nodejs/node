'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');

var referenceErrorCount = 0;

common.ArrayStream.prototype.write = function(msg) {
  if (msg.startsWith('ReferenceError: ')) {
    referenceErrorCount++;
  }
};

const putIn = new common.ArrayStream();
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
