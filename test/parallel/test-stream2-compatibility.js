'use strict';
const common = require('../common');
const R = require('_stream_readable');
const assert = require('assert');

const util = require('util');
const EE = require('events').EventEmitter;

var ondataCalled = 0;

function TestReader() {
  R.apply(this);
  this._buffer = new Buffer(100);
  this._buffer.fill('x');

  this.on('data', function() {
    ondataCalled++;
  });
}

util.inherits(TestReader, R);

TestReader.prototype._read = function(n) {
  this.push(this._buffer);
  this._buffer = new Buffer(0);
};

var reader = new TestReader();
setImmediate(function() {
  assert.equal(ondataCalled, 1);
  console.log('ok');
});
