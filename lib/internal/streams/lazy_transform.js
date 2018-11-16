// LazyTransform is a special type of Transform stream that is lazily loaded.
// This is used for performance with bi-API-ship: when two APIs are available
// for the stream, one conventional and one non-conventional.
'use strict';

const stream = require('stream');
const util = require('util');

const {
  getDefaultEncoding
} = require('internal/crypto/util');

module.exports = LazyTransform;

function LazyTransform(options) {
  this._options = options;
  this.writable = true;
  this.readable = true;
}
util.inherits(LazyTransform, stream.Transform);

function makeGetter(name) {
  return function() {
    stream.Transform.call(this, this._options);
    this._writableState.decodeStrings = false;

    if (!this._options || !this._options.defaultEncoding) {
      this._writableState.defaultEncoding = getDefaultEncoding();
    }

    return this[name];
  };
}

function makeSetter(name) {
  return function(val) {
    Object.defineProperty(this, name, {
      value: val,
      enumerable: true,
      configurable: true,
      writable: true
    });
  };
}

Object.defineProperties(LazyTransform.prototype, {
  _readableState: {
    get: makeGetter('_readableState'),
    set: makeSetter('_readableState'),
    configurable: true,
    enumerable: true
  },
  _writableState: {
    get: makeGetter('_writableState'),
    set: makeSetter('_writableState'),
    configurable: true,
    enumerable: true
  },
  _transformState: {
    get: makeGetter('_transformState'),
    set: makeSetter('_transformState'),
    configurable: true,
    enumerable: true
  }
});
