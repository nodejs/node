'use strict';
require('../common');
const assert = require('assert');
const stream = require('stream');

class MyWritable extends stream.Writable {
  _write(chunk, encoding, callback) {
    assert.notStrictEqual(chunk, null);
    callback();
  }
}

assert.throws(() => {
  var m = new MyWritable({objectMode: true});
  m.write(null, (err) => assert.ok(err));
}, TypeError, 'May not write null values to stream');
assert.doesNotThrow(() => {
  var m = new MyWritable({objectMode: true}).on('error', (e) => {
    assert.ok(e);
  });
  m.write(null, (err) => {
    assert.ok(err);
  });
});

assert.throws(() => {
  var m = new MyWritable();
  m.write(false, (err) => assert.ok(err));
}, TypeError, 'Invalid non-string/buffer chunk');
assert.doesNotThrow(() => {
  var m = new MyWritable().on('error', (e) => {
    assert.ok(e);
  });
  m.write(false, (err) => {
    assert.ok(err);
  });
});

assert.doesNotThrow(() => {
  var m = new MyWritable({objectMode: true});
  m.write(false, (err) => assert.ifError(err));
});
assert.doesNotThrow(() => {
  var m = new MyWritable({objectMode: true}).on('error', (e) => {
    assert.ifError(e || new Error('should not get here'));
  });
  m.write(false, (err) => {
    assert.ifError(err);
  });
});
