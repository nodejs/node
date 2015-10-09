var common = require('../common');
var stream = require('stream');
var assert = require('assert');

(function errorsAreForwardes() {
  var writable = new stream.Writable({
    write: function (chunk, _, cb) {
      process.nextTick(cb);
    }
  });

  var called = 0;
  writable.on('error', function (){
    called++;
  });
  writable.on('unpipe', function () {
    process.nextTick(function () {
      assert.equal(called, 1);
    });
  });

  var readable = new stream.Readable({
    read: function () {
      this.emit('error');
    }
  });
  readable.pipe(writable, {
    forwardErrors: true
  })
}());


(function CircularErrorsAreForwardes() {
  var passthrough1 = new stream.PassThrough();
  var passthrough2 = new stream.PassThrough();
  var called = 0;
  passthrough2.on('error', function (){
    called++;
  });
  passthrough2.on('unpipe', function () {
    process.nextTick(function () {
      assert.equal(called, 1);
    });
  });

  var readable = new stream.Readable({
    read: function () {
      this.emit('error');
    }
  });
  readable.pipe(passthrough1, {
    forwardErrors: true
  }).pipe(passthrough2, {
    forwardErrors: true
  }).pipe(passthrough1, {
    forwardErrors: true
  });
}())
