/* eslint-disable node-core/required-modules */
'use strict';

const { Stream } = require('stream');
const { inherits } = require('util');
function noop() {}

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    data.forEach((line) => {
      this.emit('data', `${line}\n`);
    });
  };
}

inherits(ArrayStream, Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.pause = noop;
ArrayStream.prototype.resume = noop;
ArrayStream.prototype.write = noop;

module.exports = ArrayStream;
