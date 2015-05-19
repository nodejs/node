'use strict';
var common = require('../common');
var R = require('_stream_readable');
var assert = require('assert');

var util = require('util');
var EE = require('events').EventEmitter;

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
