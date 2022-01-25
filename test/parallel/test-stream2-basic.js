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
const { Readable: R, Writable: W } = require('stream');
const assert = require('assert');

const EE = require('events').EventEmitter;

class TestReader extends R {
  constructor(n) {
    super();
    this._buffer = Buffer.alloc(n || 100, 'x');
    this._pos = 0;
    this._bufs = 10;
  }

  _read(n) {
    const max = this._buffer.length - this._pos;
    n = Math.max(n, 0);
    const toRead = Math.min(n, max);
    if (toRead === 0) {
      // Simulate the read buffer filling up with some more bytes some time
      // in the future.
      setTimeout(() => {
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
      }, 10);
      return;
    }

    const ret = this._buffer.slice(this._pos, this._pos + toRead);
    this._pos += toRead;
    this.push(ret);
  }
}

class TestWriter extends EE {
  constructor() {
    super();
    this.received = [];
    this.flush = false;
  }

  write(c) {
    this.received.push(c.toString());
    this.emit('write', c);
    return true;
  }

  end(c) {
    if (c) this.write(c);
    this.emit('end', this.received);
  }
}

{
  // Test basic functionality
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

  r.on('end', common.mustCall(function() {
    assert.deepStrictEqual(reads, expect);
  }));

  let readSize = 1;
  function flow() {
    let res;
    while (null !== (res = r.read(readSize++))) {
      reads.push(res.toString());
    }
    r.once('readable', flow);
  }

  flow();
}

{
  // Verify pipe
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

  w.on('end', common.mustCall(function(received) {
    assert.deepStrictEqual(received, expect);
  }));

  r.pipe(w);
}


[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach(function(SPLIT) {
  // Verify unpipe
  const r = new TestReader(5);

  // Unpipe after 3 writes, then write to another stream instead.
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
      assert.deepStrictEqual(r._readableState.pipes, []);
      w[0].end();
      r.pipe(w[1]);
      assert.deepStrictEqual(r._readableState.pipes, [w[1]]);
    }
  });

  let ended = 0;

  w[0].on('end', common.mustCall(function(results) {
    ended++;
    assert.strictEqual(ended, 1);
    assert.deepStrictEqual(results, expect[0]);
  }));

  w[1].on('end', common.mustCall(function(results) {
    ended++;
    assert.strictEqual(ended, 2);
    assert.deepStrictEqual(results, expect[1]);
  }));

  r.pipe(w[0]);
});


{
  // Verify both writers get the same data when piping to destinations
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

  w[0].on('end', common.mustCall(function(received) {
    assert.deepStrictEqual(received, expect);
  }));
  w[1].on('end', common.mustCall(function(received) {
    assert.deepStrictEqual(received, expect);
  }));

  r.pipe(w[0]);
  r.pipe(w[1]);
}


[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach(function(SPLIT) {
  // Verify multi-unpipe
  const r = new TestReader(5);

  // Unpipe after 3 writes, then write to another stream instead.
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

  w[0].on('end', common.mustCall(function(results) {
    ended++;
    assert.strictEqual(ended, 1);
    assert.deepStrictEqual(results, expect[0]);
  }));

  w[1].on('end', common.mustCall(function(results) {
    ended++;
    assert.strictEqual(ended, 2);
    assert.deepStrictEqual(results, expect[1]);
  }));

  r.pipe(w[0]);
  r.pipe(w[2]);
});

{
  // Verify that back pressure is respected
  const r = new R({ objectMode: true });
  r._read = common.mustNotCall();
  let counter = 0;
  r.push(['one']);
  r.push(['two']);
  r.push(['three']);
  r.push(['four']);
  r.push(null);

  const w1 = new R();
  w1.write = function(chunk) {
    assert.strictEqual(chunk[0], 'one');
    w1.emit('close');
    process.nextTick(function() {
      r.pipe(w2);
      r.pipe(w3);
    });
  };
  w1.end = common.mustNotCall();

  r.pipe(w1);

  const expected = ['two', 'two', 'three', 'three', 'four', 'four'];

  const w2 = new R();
  w2.write = function(chunk) {
    assert.strictEqual(chunk[0], expected.shift());
    assert.strictEqual(counter, 0);

    counter++;

    if (chunk[0] === 'four') {
      return true;
    }

    setTimeout(function() {
      counter--;
      w2.emit('drain');
    }, 10);

    return false;
  };
  w2.end = common.mustCall();

  const w3 = new R();
  w3.write = function(chunk) {
    assert.strictEqual(chunk[0], expected.shift());
    assert.strictEqual(counter, 1);

    counter++;

    if (chunk[0] === 'four') {
      return true;
    }

    setTimeout(function() {
      counter--;
      w3.emit('drain');
    }, 50);

    return false;
  };
  w3.end = common.mustCall(function() {
    assert.strictEqual(counter, 2);
    assert.strictEqual(expected.length, 0);
  });
}

{
  // Verify read(0) behavior for ended streams
  const r = new R();
  let written = false;
  let ended = false;
  r._read = common.mustNotCall();

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

  w.end = common.mustCall(function() {
    ended = true;
    assert.strictEqual(written, true);
  });

  r.pipe(w);
}

{
  // Verify synchronous _read ending
  const r = new R();
  let called = false;
  r._read = function(n) {
    r.push(null);
  };

  r.once('end', function() {
    // Verify that this is called before the next tick
    called = true;
  });

  r.read();

  process.nextTick(function() {
    assert.strictEqual(called, true);
  });
}

{
  // Verify that adding readable listeners trigger data flow
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

  r.on('end', common.mustCall(function() {
    assert.strictEqual(readCalled, 3);
    assert.ok(onReadable);
  }));
}

{
  // Verify that streams are chainable
  const r = new R();
  r._read = common.mustCall();
  const r2 = r.setEncoding('utf8').pause().resume().pause();
  assert.strictEqual(r, r2);
}

{
  // Verify readableEncoding property
  assert(Object.hasOwn(R.prototype, 'readableEncoding'));

  const r = new R({ encoding: 'utf8' });
  assert.strictEqual(r.readableEncoding, 'utf8');
}

{
  // Verify readableObjectMode property
  assert(Object.hasOwn(R.prototype, 'readableObjectMode'));

  const r = new R({ objectMode: true });
  assert.strictEqual(r.readableObjectMode, true);
}

{
  // Verify writableObjectMode property
  assert(Object.hasOwn(W.prototype, 'writableObjectMode'));

  const w = new W({ objectMode: true });
  assert.strictEqual(w.writableObjectMode, true);
}
