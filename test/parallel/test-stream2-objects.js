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
require('../common');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const assert = require('assert');

// tiny node-tap lookalike.
const tests = [];
let count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  const next = tests.shift();
  if (!next)
    return console.error('ok');

  const name = next[0];
  const fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepStrictEqual,
    equal: assert.strictEqual,
    end: function() {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on('exit', function() {
  assert.strictEqual(count, 0);
});

process.nextTick(run);

function toArray(callback) {
  const stream = new Writable({ objectMode: true });
  const list = [];
  stream.write = function(chunk) {
    list.push(chunk);
  };

  stream.end = function() {
    callback(list);
  };

  return stream;
}

function fromArray(list) {
  const r = new Readable({ objectMode: true });
  r._read = noop;
  list.forEach(function(chunk) {
    r.push(chunk);
  });
  r.push(null);

  return r;
}

function noop() {}

test('can read objects from stream', function(t) {
  const r = fromArray([{ one: '1'}, { two: '2' }]);

  const v1 = r.read();
  const v2 = r.read();
  const v3 = r.read();

  assert.deepStrictEqual(v1, { one: '1' });
  assert.deepStrictEqual(v2, { two: '2' });
  assert.deepStrictEqual(v3, null);

  t.end();
});

test('can pipe objects into stream', function(t) {
  const r = fromArray([{ one: '1'}, { two: '2' }]);

  r.pipe(toArray(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);

    t.end();
  }));
});

test('read(n) is ignored', function(t) {
  const r = fromArray([{ one: '1'}, { two: '2' }]);

  const value = r.read(2);

  assert.deepStrictEqual(value, { one: '1' });

  t.end();
});

test('can read objects from _read (sync)', function(t) {
  const r = new Readable({ objectMode: true });
  const list = [{ one: '1'}, { two: '2' }];
  r._read = function(n) {
    const item = list.shift();
    r.push(item || null);
  };

  r.pipe(toArray(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);

    t.end();
  }));
});

test('can read objects from _read (async)', function(t) {
  const r = new Readable({ objectMode: true });
  const list = [{ one: '1'}, { two: '2' }];
  r._read = function(n) {
    const item = list.shift();
    process.nextTick(function() {
      r.push(item || null);
    });
  };

  r.pipe(toArray(function(list) {
    assert.deepStrictEqual(list, [
      { one: '1' },
      { two: '2' }
    ]);

    t.end();
  }));
});

test('can read strings as objects', function(t) {
  const r = new Readable({
    objectMode: true
  });
  r._read = noop;
  const list = ['one', 'two', 'three'];
  list.forEach(function(str) {
    r.push(str);
  });
  r.push(null);

  r.pipe(toArray(function(array) {
    assert.deepStrictEqual(array, list);

    t.end();
  }));
});

test('read(0) for object streams', function(t) {
  const r = new Readable({
    objectMode: true
  });
  r._read = noop;

  r.push('foobar');
  r.push(null);

  r.pipe(toArray(function(array) {
    assert.deepStrictEqual(array, ['foobar']);

    t.end();
  }));
});

test('falsey values', function(t) {
  const r = new Readable({
    objectMode: true
  });
  r._read = noop;

  r.push(false);
  r.push(0);
  r.push('');
  r.push(null);

  r.pipe(toArray(function(array) {
    assert.deepStrictEqual(array, [false, 0, '']);

    t.end();
  }));
});

test('high watermark _read', function(t) {
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

  t.end();
});

test('high watermark push', function(t) {
  const r = new Readable({
    highWaterMark: 6,
    objectMode: true
  });
  r._read = function(n) {};
  for (let i = 0; i < 6; i++) {
    const bool = r.push(i);
    assert.strictEqual(bool, i !== 5);
  }

  t.end();
});

test('can write objects to stream', function(t) {
  const w = new Writable({ objectMode: true });

  w._write = function(chunk, encoding, cb) {
    assert.deepStrictEqual(chunk, { foo: 'bar' });
    cb();
  };

  w.on('finish', function() {
    t.end();
  });

  w.write({ foo: 'bar' });
  w.end();
});

test('can write multiple objects to stream', function(t) {
  const w = new Writable({ objectMode: true });
  const list = [];

  w._write = function(chunk, encoding, cb) {
    list.push(chunk);
    cb();
  };

  w.on('finish', function() {
    assert.deepStrictEqual(list, [0, 1, 2, 3, 4]);

    t.end();
  });

  w.write(0);
  w.write(1);
  w.write(2);
  w.write(3);
  w.write(4);
  w.end();
});

test('can write strings as objects', function(t) {
  const w = new Writable({
    objectMode: true
  });
  const list = [];

  w._write = function(chunk, encoding, cb) {
    list.push(chunk);
    process.nextTick(cb);
  };

  w.on('finish', function() {
    assert.deepStrictEqual(list, ['0', '1', '2', '3', '4']);

    t.end();
  });

  w.write('0');
  w.write('1');
  w.write('2');
  w.write('3');
  w.write('4');
  w.end();
});

test('buffers finish until cb is called', function(t) {
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

  w.on('finish', function() {
    assert.strictEqual(called, true);

    t.end();
  });

  w.write('foo');
  w.end();
});
