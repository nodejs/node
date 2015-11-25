'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const repl = require('repl');
const util = require('util');
let found = false;

process.on('exit', () => {
  assert.strictEqual(found, true);
});

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    data.forEach(line => {
      this.emit('data', line + '\n');
    });
  };
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function(output) {
  if (/var foo bar;/.test(output))
    found = true;
};

const putIn = new ArrayStream();
const testMe = repl.start('', putIn);
let file = path.resolve(__dirname, '../fixtures/syntax/bad_syntax');

if (common.isWindows)
  file = file.replace(/\\/g, '\\\\');

putIn.run(['.clear']);
putIn.run([`require('${file}');`]);
