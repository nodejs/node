'use strict';
const {
  Symbol,
} = primordials;

const { Duplex } = require('stream');
const assert = require('internal/assert');

const kCallback = Symbol('Callback');
const kInitOtherSide = Symbol('InitOtherSide');

class DuplexSide extends Duplex {
  #otherSide = null;

  constructor(options) {
    super(options);
    this[kCallback] = null;
    this.#otherSide = null;
  }

  [kInitOtherSide](otherSide) {
    // Ensure this can only be set once, to enforce encapsulation.
    if (this.#otherSide === null) {
      this.#otherSide = otherSide;
    } else {
      assert(this.#otherSide === null);
    }
  }

  _read() {
    const callback = this[kCallback];
    if (callback) {
      this[kCallback] = null;
      callback();
    }
  }

  _write(chunk, encoding, callback) {
    assert(this.#otherSide !== null);
    assert(this.#otherSide[kCallback] === null);
    if (chunk.length === 0) {
      process.nextTick(callback);
    } else {
      this.#otherSide.push(chunk);
      this.#otherSide[kCallback] = callback;
    }
  }

  _final(callback) {
    this.#otherSide.on('end', callback);
    this.#otherSide.push(null);
  }


  _destroy(err, callback) {
    const otherSide = this.#otherSide;

    if (otherSide !== null && !otherSide.destroyed) {
      // Use nextTick to avoid crashing the current execution stack (like HTTP parser)
      process.nextTick(() => {
        if (otherSide.destroyed) return;

        if (err) {
          // Destroy the other side, without passing the 'err' object.
          // This closes the other side gracefully so it doesn't hang,
          // but prevents the "Unhandled error" crash.
          otherSide.destroy();
        } else {
          // Standard graceful close
          otherSide.push(null);
        }
      });
    }

    callback(err);
  }
}

function duplexPair(options) {
  const side0 = new DuplexSide(options);
  const side1 = new DuplexSide(options);
  side0[kInitOtherSide](side1);
  side1[kInitOtherSide](side0);
  return [side0, side1];
}
module.exports = duplexPair;
