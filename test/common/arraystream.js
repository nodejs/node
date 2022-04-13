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

module.exports = ArrayStream;
