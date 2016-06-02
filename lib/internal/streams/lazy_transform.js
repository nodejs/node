// LazyTransform is a special type of Transform stream that is lazily loaded.
// This is used for performance with bi-API-ship: when two APIs are available
// for the stream, one conventional and one non-conventional.
'use strict';

const stream = require('stream');
const util = require('util');

module.exports = LazyTransform;

function LazyTransform(options) {
  this._options = options;
}
util.inherits(LazyTransform, stream.Transform);

[
  '_readableState',
  '_writableState',
  '_transformState'
].forEach(function(prop, i, props) {
  Object.defineProperty(LazyTransform.prototype, prop, {
    get: function() {
      stream.Transform.call(this, this._options);
      this._writableState.decodeStrings = false;
      this._writableState.defaultEncoding = 'latin1';
      return this[prop];
    },
    set: function(val) {
      Object.defineProperty(this, prop, {
        value: val,
        enumerable: true,
        configurable: true,
        writable: true
      });
    },
    configurable: true,
    enumerable: true
  });
});
