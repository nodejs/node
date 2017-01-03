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
const R = require('_stream_readable');
const assert = require('assert');

const util = require('util');
const EE = require('events').EventEmitter;

function TestReader(n) {
  R.apply(this);
  this._buffer = Buffer.alloc(n || 100, 'x');
  this._pos = 0;
  this._bufs = 10;
}

util.inherits(TestReader, R);

TestReader.prototype._read = function(n) {
  const max = this._buffer.length - this._pos;
  n = Math.max(n, 0);
  const toRead = Math.min(n, max);
  if (toRead === 0) {
    // simulate the read buffer filling up with some more bytes some time
    // in the future.
    setTimeout(function() {
      this._pos = 0;
      this._bufs -= 1;
      if (this._bufs <= 0) {
        // read them all!
        if (!this.ended)
          this.push(null);
      } else {
        // now we have more.
        // kinda cheating by calling _read, but whatever,
        // it's just fake anyway.
        this._read(n);
      }
    }.bind(this), 10);
    return;
  }

  const ret = this._buffer.slice(this._pos, this._pos + toRead);
  this._pos += toRead;
  this.push(ret);
};

/////

function TestWriter() {
  EE.apply(this);
  this.received = [];
  this.flush = false;
}

util.inherits(TestWriter, EE);

TestWriter.prototype.write = function(c) {
  this.received.push(c.toString());
  this.emit('write', c);
  return true;
};

TestWriter.prototype.end = function(c) {
  if (c) this.write(c);
  this.emit('end', this.received);
};

////////

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
    ok: assert,
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


test('a most basic test', function(t) {
  const r = new TestReader(20);

  const reads = [];
  const expect = [ 'x',
                   'xx',
                   'xxx',
                   'xxxx',
                   'xxxxx',
                   'xxxxxxxxx',
                   'xxxxxxxxxx',
                   'xxxxxxxxxxxx',
                   'xxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxxxxxxxxxx',
                   'xxxxxxxxxxxxxxxxxxxxx' ];

  r.on('end', function() {
    t.same(reads, expect);
    t.end();
  });

  let readSize = 1;
  function flow() {
    let res;
    while (null !== (res = r.read(readSize++))) {
      reads.push(res.toString());
    }
    r.once('readable', flow);
  }

  flow();
});

test('pipe', function(t) {
  const r = new TestReader(5);

  const expect = [ 'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx' ];

  const w = new TestWriter();

  w.on('end', function(received) {
    t.same(received, expect);
    t.end();
  });

  r.pipe(w);
});


[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach(function(SPLIT) {
  test('unpipe', function(t) {
    const r = new TestReader(5);

    // unpipe after 3 writes, then write to another stream instead.
    let expect = [ 'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx' ];
    expect = [ expect.slice(0, SPLIT), expect.slice(SPLIT) ];

    const w = [ new TestWriter(), new TestWriter() ];

    let writes = SPLIT;
    w[0].on('write', function() {
      if (--writes === 0) {
        r.unpipe();
        t.equal(r._readableState.pipes, null);
        w[0].end();
        r.pipe(w[1]);
        t.equal(r._readableState.pipes, w[1]);
      }
    });

    let ended = 0;

    let ended0 = false;
    let ended1 = false;
    w[0].on('end', function(results) {
      t.equal(ended0, false);
      ended0 = true;
      ended++;
      t.same(results, expect[0]);
    });

    w[1].on('end', function(results) {
      t.equal(ended1, false);
      ended1 = true;
      ended++;
      t.equal(ended, 2);
      t.same(results, expect[1]);
      t.end();
    });

    r.pipe(w[0]);
  });
});


// both writers should get the same exact data.
test('multipipe', function(t) {
  const r = new TestReader(5);
  const w = [ new TestWriter(), new TestWriter() ];

  const expect = [ 'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx' ];

  let c = 2;
  w[0].on('end', function(received) {
    t.same(received, expect, 'first');
    if (--c === 0) t.end();
  });
  w[1].on('end', function(received) {
    t.same(received, expect, 'second');
    if (--c === 0) t.end();
  });

  r.pipe(w[0]);
  r.pipe(w[1]);
});


[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach(function(SPLIT) {
  test('multi-unpipe', function(t) {
    const r = new TestReader(5);

    // unpipe after 3 writes, then write to another stream instead.
    let expect = [ 'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx',
                   'xxxxx' ];
    expect = [ expect.slice(0, SPLIT), expect.slice(SPLIT) ];

    const w = [ new TestWriter(), new TestWriter(), new TestWriter() ];

    let writes = SPLIT;
    w[0].on('write', function() {
      if (--writes === 0) {
        r.unpipe();
        w[0].end();
        r.pipe(w[1]);
      }
    });

    let ended = 0;

    w[0].on('end', function(results) {
      ended++;
      t.same(results, expect[0]);
    });

    w[1].on('end', function(results) {
      ended++;
      t.equal(ended, 2);
      t.same(results, expect[1]);
      t.end();
    });

    r.pipe(w[0]);
    r.pipe(w[2]);
  });
});

test('back pressure respected', function(t) {
  function noop() {}

  const r = new R({ objectMode: true });
  r._read = noop;
  let counter = 0;
  r.push(['one']);
  r.push(['two']);
  r.push(['three']);
  r.push(['four']);
  r.push(null);

  const w1 = new R();
  w1.write = function(chunk) {
    console.error('w1.emit("close")');
    assert.strictEqual(chunk[0], 'one');
    w1.emit('close');
    process.nextTick(function() {
      r.pipe(w2);
      r.pipe(w3);
    });
  };
  w1.end = noop;

  r.pipe(w1);

  const expected = ['two', 'two', 'three', 'three', 'four', 'four'];

  const w2 = new R();
  w2.write = function(chunk) {
    console.error('w2 write', chunk, counter);
    assert.strictEqual(chunk[0], expected.shift());
    assert.strictEqual(counter, 0);

    counter++;

    if (chunk[0] === 'four') {
      return true;
    }

    setTimeout(function() {
      counter--;
      console.error('w2 drain');
      w2.emit('drain');
    }, 10);

    return false;
  };
  w2.end = noop;

  const w3 = new R();
  w3.write = function(chunk) {
    console.error('w3 write', chunk, counter);
    assert.strictEqual(chunk[0], expected.shift());
    assert.strictEqual(counter, 1);

    counter++;

    if (chunk[0] === 'four') {
      return true;
    }

    setTimeout(function() {
      counter--;
      console.error('w3 drain');
      w3.emit('drain');
    }, 50);

    return false;
  };
  w3.end = function() {
    assert.strictEqual(counter, 2);
    assert.strictEqual(expected.length, 0);
    t.end();
  };
});

test('read(0) for ended streams', function(t) {
  const r = new R();
  let written = false;
  let ended = false;
  r._read = function(n) {};

  r.push(Buffer.from('foo'));
  r.push(null);

  const v = r.read(0);

  assert.strictEqual(v, null);

  const w = new R();

  w.write = function(buffer) {
    written = true;
    assert.strictEqual(ended, false);
    assert.strictEqual(buffer.toString(), 'foo');
  };

  w.end = function() {
    ended = true;
    assert.strictEqual(written, true);
    t.end();
  };

  r.pipe(w);
});

test('sync _read ending', function(t) {
  const r = new R();
  let called = false;
  r._read = function(n) {
    r.push(null);
  };

  r.once('end', function() {
    called = true;
  });

  r.read();

  process.nextTick(function() {
    assert.strictEqual(called, true);
    t.end();
  });
});

test('adding readable triggers data flow', function(t) {
  const r = new R({ highWaterMark: 5 });
  let onReadable = false;
  let readCalled = 0;

  r._read = function(n) {
    if (readCalled++ === 2)
      r.push(null);
    else
      r.push(Buffer.from('asdf'));
  };

  r.on('readable', function() {
    onReadable = true;
    r.read();
  });

  r.on('end', function() {
    t.equal(readCalled, 3);
    t.ok(onReadable);
    t.end();
  });
});

test('chainable', function(t) {
  const r = new R();
  r._read = function() {};
  const r2 = r.setEncoding('utf8').pause().resume().pause();
  t.equal(r, r2);
  t.end();
});
