'use strict';
require('../common');
var assert = require('assert');

var Readable = require('stream').Readable;
var util = require('util');

util.inherits(MyStream, Readable);
function MyStream(options) {
  Readable.call(this, options);
  this._chunks = 3;
}

MyStream.prototype._read = function(n) {
  switch (this._chunks--) {
    case 0:
      return this.push(null);
    case 1:
      return setTimeout(function() {
        this.push('last chunk');
      }.bind(this), 100);
    case 2:
      return this.push('second to last chunk');
    case 3:
      return process.nextTick(function() {
        this.push('first chunk');
      }.bind(this));
    default:
      throw new Error('?');
  }
};

var ms = new MyStream();
var results = [];
ms.on('readable', function() {
  var chunk;
  while (null !== (chunk = ms.read()))
    results.push(chunk + '');
});

var expect = [ 'first chunksecond to last chunk', 'last chunk' ];
process.on('exit', function() {
  assert.equal(ms._chunks, -1);
  assert.deepStrictEqual(results, expect);
  console.log('ok');
});
