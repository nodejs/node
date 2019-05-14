'use strict';
const { Duplex } = require('stream');

const kCallback = Symbol('Callback');
const kOtherSide = Symbol('Other');

class DuplexSocket extends Duplex {
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
    this[kOtherSide][kCallback] = callback;
    this[kOtherSide].push(chunk);
  }

  _final(callback) {
    this[kOtherSide].on('end', callback);
    this[kOtherSide].push(null);
  }
}

class DuplexPair {
  constructor() {
    this.socket1 = new DuplexSocket();
    this.socket2 = new DuplexSocket();
    this.socket1[kOtherSide] = this.socket2;
    this.socket2[kOtherSide] = this.socket1;
  }
}

module.exports = DuplexPair;
