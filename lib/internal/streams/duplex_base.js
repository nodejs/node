'use strict';

const util = require('util');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');

function DuplexBase(options) {
  Readable.call(this, options);
  Writable.call(this, options);

  if (options && options.readable === false)
    this.readable = false;

  if (options && options.writable === false)
    this.writable = false;
}

util.inherits(DuplexBase, Readable);

{
  // Avoid scope creep, the keys array can then be collected.
  const keys = Object.keys(Writable.prototype);
  for (var v = 0; v < keys.length; v++) {
    const method = keys[v];
    if (!DuplexBase.prototype[method])
      DuplexBase.prototype[method] = Writable.prototype[method];
  }
}

module.exports = DuplexBase;
