// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const assert = require('assert');

function toArray(callback) {
  const stream = new Writable({ objectMode: true });
  const list = [];
  stream.write = function(chunk) {
    list.push(chunk);
  };

  stream.end = common.mustCall(function() {
    callback(list);
  });

  return stream;
}

function fromArray(list) {
  const r = new Readable({ objectMode: true });
  r._read = common.mustNotCall();
  list.forEach(function(chunk) {
    r.push(chunk);
  });
  r.push(null);

  return r;
}

{
  // Verify that objects can be read from the stream
  const r = fromArray([{ one: '1'}, { two: '2' }]);

  const v1 = r.read();
  const v2 = r.read();
  const v3 = r.read();

  assert.deepStrictEqual(v1, { one: '1' });
  assert.deepStrictEqual(v2, { two: '2' });
  assert.deepStrictEqual(v3, null);
}

{
  // Verify that objects can be piped into the stream
  const r = fromArray([{ one: '1'}, { two: '2' }]);

  r.pipe(toArray(common.mustCall(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);
  })));
}

{
  // Verify that read(n) is ignored
  const r = fromArray([{ one: '1'}, { two: '2' }]);
  const value = r.read(2);

  assert.deepStrictEqual(value, { one: '1' });
}

{
  // Verify that objects can be synchronously read
  const r = new Readable({ objectMode: true });
  const list = [{ one: '1'}, { two: '2' }];
  r._read = function(n) {
    const item = list.shift();
    r.push(item || null);
  };

  r.pipe(toArray(common.mustCall(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);
  })));
}

{
  // Verify that objects can be asynchronously read
  const r = new Readable({ objectMode: true });
  const list = [{ one: '1'}, { two: '2' }];
  r._read = function(n) {
    const item = list.shift();
    process.nextTick(function() {
      r.push(item || null);
    });
  };

  r.pipe(toArray(common.mustCall(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);
  })));
}

{
  // Verify that strings can be read as objects
  const r = new Readable({
    objectMode: true
  });
  r._read = common.mustNotCall();
  const list = ['one', 'two', 'three'];
  list.forEach(function(str) {
    r.push(str);
  });
  r.push(null);

  r.pipe(toArray(common.mustCall(function(array) {
    assert.deepStrictEqual(array, list);
  })));
}

{
  // Verify read(0) behavior for object streams
  const r = new Readable({
    objectMode: true
  });
  r._read = common.mustNotCall();

  r.push('foobar');
  r.push(null);

  r.pipe(toArray(common.mustCall(function(array) {
    assert.deepStrictEqual(array, ['foobar']);
  })));
}

{
  // Verify the behavior of pushing falsey values
  const r = new Readable({
    objectMode: true
  });
  r._read = common.mustNotCall();

  r.push(false);
  r.push(0);
  r.push('');
  r.push(null);

  r.pipe(toArray(common.mustCall(function(array) {
    assert.deepStrictEqual(array, [false, 0, '']);
  })));
}

{
  // Verify high watermark _read() behavior
  const r = new Readable({
    highWaterMark: 6,
    objectMode: true
  });
  let calls = 0;
  const list = ['1', '2', '3', '4', '5', '6', '7', '8'];

  r._read = function(n) {
    calls++;
  };

  list.forEach(function(c) {
    r.push(c);
  });

  const v = r.read();

  assert.strictEqual(calls, 0);
  assert.strictEqual(v, '1');

  const v2 = r.read();
  assert.strictEqual(v2, '2');

  const v3 = r.read();
  assert.strictEqual(v3, '3');

  assert.strictEqual(calls, 1);
}

{
  // Verify high watermark push behavior
  const r = new Readable({
    highWaterMark: 6,
    objectMode: true
  });
  r._read = common.mustNotCall();
  for (let i = 0; i < 6; i++) {
    const bool = r.push(i);
    assert.strictEqual(bool, i !== 5);
  }
}

{
  // Verify that objects can be written to stream
  const w = new Writable({ objectMode: true });

  w._write = function(chunk, encoding, cb) {
    assert.deepStrictEqual(chunk, { foo: 'bar' });
    cb();
  };

  w.on('finish', common.mustCall());
  w.write({ foo: 'bar' });
  w.end();
}

{
  // Verify that multiple objects can be written to stream
  const w = new Writable({ objectMode: true });
  const list = [];

  w._write = function(chunk, encoding, cb) {
    list.push(chunk);
    cb();
  };

  w.on('finish', common.mustCall(function() {
    assert.deepStrictEqual(list, [0, 1, 2, 3, 4]);
  }));

  w.write(0);
  w.write(1);
  w.write(2);
  w.write(3);
  w.write(4);
  w.end();
}

{
  // Verify that strings can be written as objects
  const w = new Writable({
    objectMode: true
  });
  const list = [];

  w._write = function(chunk, encoding, cb) {
    list.push(chunk);
    process.nextTick(cb);
  };

  w.on('finish', common.mustCall(function() {
    assert.deepStrictEqual(list, ['0', '1', '2', '3', '4']);
  }));

  w.write('0');
  w.write('1');
  w.write('2');
  w.write('3');
  w.write('4');
  w.end();
}

{
  // Verify that stream buffers finish until callback is called
  const w = new Writable({
    objectMode: true
  });
  let called = false;

  w._write = function(chunk, encoding, cb) {
    assert.strictEqual(chunk, 'foo');

    process.nextTick(function() {
      called = true;
      cb();
    });
  };

  w.on('finish', common.mustCall(function() {
    assert.strictEqual(called, true);
  }));

  w.write('foo');
  w.end();
}
