'use strict';
const { Duplex } = require('stream');
const assert = require('internal/assert');

const { Symbol } = primordials;
const kCallback = Symbol('Callback');
const kOtherSide = Symbol('Other');

class DuplexSide extends Duplex {
  constructor() {
    super();
    this[kCallback] = null;
    this[kOtherSide] = null;
  }

  _read() {
    const callback = this[kCallback];
    if (callback) {
      this[kCallback] = null;
      callback();
    }
  }

  _write(chunk, encoding, callback) {
    assert(this[kOtherSide] !== null);
    assert(this[kOtherSide][kCallback] === null);
    if (chunk.length === 0) {
      process.nextTick(callback);
    } else {
      this[kOtherSide].push(chunk);
      this[kOtherSide][kCallback] = callback;
    }
  }

  _final(callback) {
    this[kOtherSide].on('end', callback);
    this[kOtherSide].push(null);
  }
}

function duplexPair(options) {
  const side0 = new DuplexSide(options);
  const side1 = new DuplexSide(options);
  side0[kOtherSide] = side1;
  side1[kOtherSide] = side0;
  return [ side0, side1 ];
}
module.exports = duplexPair;
