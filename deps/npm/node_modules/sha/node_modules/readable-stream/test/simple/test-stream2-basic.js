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


var common = require('../common.js');
var R = require('../../lib/_stream_readable');
var assert = require('assert');

var util = require('util');
var EE = require('events').EventEmitter;

function TestReader(n) {
  R.apply(this);
  this._buffer = new Buffer(n || 100);
  this._buffer.fill('x');
  this._pos = 0;
  this._bufs = 10;
}

util.inherits(TestReader, R);

TestReader.prototype.read = function(n) {
  if (n === 0) return null;
  var max = this._buffer.length - this._pos;
  n = n || max;
  n = Math.max(n, 0);
  var toRead = Math.min(n, max);
  if (toRead === 0) {
    // simulate the read buffer filling up with some more bytes some time
    // in the future.
    setTimeout(function() {
      this._pos = 0;
      this._bufs -= 1;
      if (this._bufs <= 0) {
        // read them all!
        if (!this.ended) {
          this.emit('end');
          this.ended = true;
        }
      } else {
        this.emit('readable');
      }
    }.bind(this), 10);
    return null;
  }

  var ret = this._buffer.slice(this._pos, this._pos + toRead);
  this._pos += toRead;
  return ret;
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
var tests = [];
var count = 0;

function test(name, fn) {
  count++;
  tests.push([name, fn]);
}

function run() {
  var next = tests.shift();
  if (!next)
    return console.error('ok');

  var name = next[0];
  var fn = next[1];
  console.log('# %s', name);
  fn({
    same: assert.deepEqual,
    ok: assert,
    equal: assert.equal,
    end: function () {
      count--;
      run();
    }
  });
}

// ensure all tests have run
process.on("exit", function () {
  assert.equal(count, 0);
});

process.nextTick(run);


test('a most basic test', function(t) {
  var r = new TestReader(20);

  var reads = [];
  var expect = [ 'x',
                 'xx',
                 'xxx',
                 'xxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxxxxx',
                 'xxxxxxxxx',
                 'xxx',
                 'xxxxxxxxxxxx',
                 'xxxxxxxx',
                 'xxxxxxxxxxxxxxx',
                 'xxxxx',
                 'xxxxxxxxxxxxxxxxxx',
                 'xx',
                 'xxxxxxxxxxxxxxxxxxxx',
                 'xxxxxxxxxxxxxxxxxxxx',
                 'xxxxxxxxxxxxxxxxxxxx',
                 'xxxxxxxxxxxxxxxxxxxx',
                 'xxxxxxxxxxxxxxxxxxxx' ];

  r.on('end', function() {
    t.same(reads, expect);
    t.end();
  });

  var readSize = 1;
  function flow() {
    var res;
    while (null !== (res = r.read(readSize++))) {
      reads.push(res.toString());
    }
    r.once('readable', flow);
  }

  flow();
});

test('pipe', function(t) {
  var r = new TestReader(5);

  var expect = [ 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx' ]

  var w = new TestWriter;
  var flush = true;

  w.on('end', function(received) {
    t.same(received, expect);
    t.end();
  });

  r.pipe(w);
});



[1,2,3,4,5,6,7,8,9].forEach(function(SPLIT) {
  test('unpipe', function(t) {
    var r = new TestReader(5);

    // unpipe after 3 writes, then write to another stream instead.
    var expect = [ 'xxxxx',
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

    var w = [ new TestWriter(), new TestWriter() ];

    var writes = SPLIT;
    w[0].on('write', function() {
      if (--writes === 0) {
        r.unpipe();
        t.equal(r._readableState.pipes, null);
        w[0].end();
        r.pipe(w[1]);
        t.equal(r._readableState.pipes, w[1]);
      }
    });

    var ended = 0;

    var ended0 = false;
    var ended1 = false;
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
  var r = new TestReader(5);
  var w = [ new TestWriter, new TestWriter ];

  var expect = [ 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx',
                 'xxxxx' ];

  var c = 2;
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


[1,2,3,4,5,6,7,8,9].forEach(function(SPLIT) {
  test('multi-unpipe', function(t) {
    var r = new TestReader(5);

    // unpipe after 3 writes, then write to another stream instead.
    var expect = [ 'xxxxx',
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

    var w = [ new TestWriter(), new TestWriter(), new TestWriter() ];

    var writes = SPLIT;
    w[0].on('write', function() {
      if (--writes === 0) {
        r.unpipe();
        w[0].end();
        r.pipe(w[1]);
      }
    });

    var ended = 0;

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

test('back pressure respected', function (t) {
  function noop() {}

  var r = new R({ objectMode: true });
  r._read = noop;
  var counter = 0;
  r.push(["one"]);
  r.push(["two"]);
  r.push(["three"]);
  r.push(["four"]);
  r.push(null);

  var w1 = new R();
  w1.write = function (chunk) {
    assert.equal(chunk[0], "one");
    w1.emit("close");
    process.nextTick(function () {
      r.pipe(w2);
      r.pipe(w3);
    })
  };
  w1.end = noop;

  r.pipe(w1);

  var expected = ["two", "two", "three", "three", "four", "four"];

  var w2 = new R();
  w2.write = function (chunk) {
    assert.equal(chunk[0], expected.shift());
    assert.equal(counter, 0);

    counter++;

    if (chunk[0] === "four") {
      return true;
    }

    setTimeout(function () {
      counter--;
      w2.emit("drain");
    }, 10);

    return false;
  }
  w2.end = noop;

  var w3 = new R();
  w3.write = function (chunk) {
    assert.equal(chunk[0], expected.shift());
    assert.equal(counter, 1);

    counter++;

    if (chunk[0] === "four") {
      return true;
    }

    setTimeout(function () {
      counter--;
      w3.emit("drain");
    }, 50);

    return false;
  };
  w3.end = function () {
    assert.equal(counter, 2);
    assert.equal(expected.length, 0);
    t.end();
  };
});

test('read(0) for ended streams', function (t) {
  var r = new R();
  var written = false;
  var ended = false;
  r._read = function (n) {};

  r.push(new Buffer("foo"));
  r.push(null);

  var v = r.read(0);

  assert.equal(v, null);

  var w = new R();

  w.write = function (buffer) {
    written = true;
    assert.equal(ended, false);
    assert.equal(buffer.toString(), "foo")
  };

  w.end = function () {
    ended = true;
    assert.equal(written, true);
    t.end();
  };

  r.pipe(w);
})

test('sync _read ending', function (t) {
  var r = new R();
  var called = false;
  r._read = function (n) {
    r.push(null);
  };

  r.once('end', function () {
    called = true;
  })

  r.read();

  process.nextTick(function () {
    assert.equal(called, true);
    t.end();
  })
});

test('adding readable triggers data flow', function(t) {
  var r = new R({ highWaterMark: 5 });
  var onReadable = false;
  var readCalled = 0;

  r._read = function(n) {
    if (readCalled++ === 2)
      r.push(null);
    else
      r.push(new Buffer('asdf'));
  };

  var called = false;
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
