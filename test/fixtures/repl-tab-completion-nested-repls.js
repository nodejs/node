// Tab completion sometimes uses a separate REPL instance under the hood.
// Make sure errors in completion callbacks are properly thrown.
//
// Ref: https://github.com/nodejs/node/issues/21586

'use strict';

const { Stream } = require('stream');
function noop() {}

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    data.forEach((line) => {
      this.emit('data', `${line}\n`);
    });
  };
}

Object.setPrototypeOf(ArrayStream.prototype, Stream.prototype);
Object.setPrototypeOf(ArrayStream, Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.pause = noop;
ArrayStream.prototype.resume = noop;
ArrayStream.prototype.write = noop;

const repl = require('repl');

const putIn = new ArrayStream();
const testMe = repl.start('', putIn);

// Nesting of structures causes REPL to use a nested REPL for completion.
putIn.run([
  'var top = function() {',
  'r = function test (',
  ' one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);

// In Node.js 10.11.0, this next line will terminate the repl silently...
testMe.complete('inner.o', () => { throw new Error('fhqwhgads'); });