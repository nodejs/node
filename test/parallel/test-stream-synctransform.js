'use strict';

const common = require('../common');
const assert = require('assert');
const { test } = require('node:test');
const {
  Readable,
  Writable,
  SyncTransform,
  pipeline,
} = require('stream');
const fs = require('fs');

// Helper functions
function stringFrom(chunks) {
  return new Readable({
    read() {
      this.push(chunks.shift() || null);
    },
  });
}

function stringSink(expected, callback) {
  return new Writable({
    write(chunk, enc, cb) {
      assert.strictEqual(chunk.toString(), expected.shift().toString());
      cb();
    },
    final(cb) {
      if (callback) callback();
      cb();
    },
  });
}

function delayedStringSink(expected, callback) {
  return new Writable({
    highWaterMark: 2,
    write(chunk, enc, cb) {
      assert.strictEqual(chunk.toString(), expected.shift().toString());
      setImmediate(cb);
    },
    final(cb) {
      if (callback) callback();
      cb();
    },
  });
}

function objectFrom(chunks) {
  return new Readable({
    objectMode: true,
    read() {
      this.push(chunks.shift() || null);
    },
  });
}

function objectSink(expected, callback) {
  return new Writable({
    objectMode: true,
    write(chunk, enc, cb) {
      assert.deepStrictEqual(chunk, expected.shift());
      cb();
    },
    final(cb) {
      if (callback) callback();
      cb();
    },
  });
}

test('SyncTransform - pipe', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    const sink = stringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - multiple pipe', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  const stream2 = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toLowerCase());
  });

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('foo'), Buffer.from('bar')];

  await new Promise((resolve) => {
    const sink = stringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(stream2).pipe(sink);
  });
});

test('SyncTransform - backpressure', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    const sink = delayedStringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - multiple pipe with backpressure', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  const stream2 = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toLowerCase());
  });

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar'), Buffer.from('baz')]);
  const expected = [Buffer.from('foo'), Buffer.from('bar'), Buffer.from('baz')];

  await new Promise((resolve) => {
    const sink = delayedStringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(stream2).pipe(sink);
  });
});

test('SyncTransform - objects', async () => {
  const stream = SyncTransform(function(chunk) {
    return { chunk };
  });
  const from = objectFrom([{ name: 'matteo' }, { answer: 42 }]);
  const expected = [{ chunk: { name: 'matteo' } }, { chunk: { answer: 42 } }];

  await new Promise((resolve) => {
    const sink = objectSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - pipe event', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  stream.on('pipe', common.mustCall((s) => {
    assert.strictEqual(s, from);
  }));

  await new Promise((resolve) => {
    const sink = stringSink(expected, resolve);
    sink.on('pipe', common.mustCall((s) => {
      assert.strictEqual(s, stream);
    }));
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - unpipe event', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = new Readable({ read() { } });
  const expected = [Buffer.from('FOO')];

  await new Promise((resolve) => {
    const sink = stringSink(expected);
    sink.on('unpipe', common.mustCall((s) => {
      assert.strictEqual(s, stream);
      resolve();
    }));

    from.pipe(stream).pipe(sink);
    from.push(Buffer.from('foo'));
    process.nextTick(() => {
      stream.unpipe(sink);
      from.push(Buffer.from('bar'));
    });
  });
});

test('SyncTransform - data event', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), expected.shift().toString());
    }, 2));

    stream.on('end', common.mustCall(resolve));
    from.pipe(stream);
  });
});

test('SyncTransform - end event during pipe', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    stream.on('end', common.mustCall(resolve));
    const sink = stringSink(expected);
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - end()', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const expected = [Buffer.from('FOO')];

  await new Promise((resolve) => {
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), expected.shift().toString());
    }));

    stream.on('end', common.mustCall(resolve));
    stream.end(Buffer.from('foo'));
  });
});

test('SyncTransform - on(\'data\') after end()', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const expected = [Buffer.from('FOO')];

  stream.end(Buffer.from('foo'));

  await new Promise((resolve) => {
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), expected.shift().toString());
    }));

    stream.on('end', common.mustCall(resolve));
  });
});

test('SyncTransform - double end()', async () => {
  const stream = SyncTransform();
  stream.end('hello');

  await new Promise((resolve) => {
    stream.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_STREAM_WRITE_AFTER_END');
      resolve();
    }));
    stream.end('world');
  });
});

test('SyncTransform - uppercase a file with on(\'data\')', async () => {
  let str = '';
  let expected = '';

  const stream = SyncTransform(function(chunk) {
    return chunk.toString().toUpperCase();
  });

  const fromPath = __filename;

  await new Promise((resolve) => {
    stream.on('data', (chunk) => {
      str += chunk;
    });

    const from = fs.createReadStream(fromPath);
    from.pipe(new Writable({
      write(chunk, enc, cb) {
        expected += chunk.toString().toUpperCase();
        cb();
      },
    })).on('finish', () => {
      assert.strictEqual(str, expected);
      resolve();
    });
    from.pipe(stream);
  });
});

test('SyncTransform - uppercase a file with pipe()', async () => {
  let str = '';
  let expected = '';

  const stream = SyncTransform(function(chunk) {
    return chunk.toString().toUpperCase();
  });

  stream.pipe(new Writable({
    objectMode: true,
    write(chunk, enc, cb) {
      str += chunk;
      cb();
    },
  }));

  const fromPath = __filename;

  await new Promise((resolve) => {
    const from = fs.createReadStream(fromPath);
    from.pipe(new Writable({
      write(chunk, enc, cb) {
        expected += chunk.toString().toUpperCase();
        cb();
      },
    })).on('finish', () => {
      assert.strictEqual(str, expected);
      resolve();
    });

    from.pipe(stream);
  });
});

test('SyncTransform - destroy()', async () => {
  const stream = SyncTransform();
  stream.destroy();

  await new Promise((resolve) => {
    stream.on('close', common.mustCall(resolve));
  });
});

test('SyncTransform - destroy(err)', async () => {
  const stream = SyncTransform();
  stream.destroy(new Error('kaboom'));

  await new Promise((resolve) => {
    stream.on('error', common.mustCall((err) => {
      assert.strictEqual(err.message, 'kaboom');
      resolve();
    }));
  });
});

test('SyncTransform - works with pipeline', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  const stream2 = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toLowerCase());
  });

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('foo'), Buffer.from('bar')];

  await new Promise((resolve, reject) => {
    const sink = stringSink(expected);
    pipeline(from, stream, stream2, sink, (err) => {
      if (err) reject(err);
      else resolve();
    });
  });
});

test('SyncTransform - works with pipeline and handles errors', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  stream.on('close', common.mustCall());

  const stream2 = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toLowerCase());
  });

  stream2.on('close', common.mustCall());

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const sink = new Writable({
    write(chunk, enc, cb) {
      cb(new Error('kaboom'));
    },
  });

  await new Promise((resolve) => {
    pipeline(from, stream, stream2, sink, (err) => {
      assert.ok(err);
      resolve();
    });
  });
});

test('SyncTransform - avoid ending pipe destination if { end: false }', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    const sink = stringSink(expected);
    sink.on('finish', common.mustNotCall());

    from.pipe(stream).pipe(sink, { end: false });

    stream.on('end', () => {
      setImmediate(resolve);
    });
  });
});

test('SyncTransform - this.push', async () => {
  const stream = SyncTransform(function(chunk) {
    this.push(Buffer.from(chunk.toString().toUpperCase()));
    this.push(Buffer.from(chunk.toString()));
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('foo'), Buffer.from('BAR'), Buffer.from('bar')];

  await new Promise((resolve) => {
    const sink = stringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - this.push objects', async () => {
  const stream = SyncTransform(function(chunks) {
    return chunks;
  });
  const from = objectFrom([{ num: 1 }, { num: 2 }, { num: 3 }, { num: 4 }, { num: 5 }, { num: 6 }]);
  const expected = [{ num: 1 }, { num: 2 }, { num: 3 }, { num: 4 }, { num: 5 }, { num: 6 }];

  await new Promise((resolve) => {
    const sink = objectSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - backpressure with push', async () => {
  let wait = false;

  const stream = SyncTransform(function(chunk) {
    assert.strictEqual(wait, false);
    wait = true;
    this.push(Buffer.from(chunk.toString().toUpperCase()));
    this.push(Buffer.from(chunk.toString()));
    setImmediate(() => {
      wait = false;
    });
  });

  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('foo'), Buffer.from('BAR'), Buffer.from('bar')];

  await new Promise((resolve) => {
    const sink = delayedStringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - returning null ends the stream', async () => {
  const stream = SyncTransform(function() {
    return null;
  });

  stream.on('data', common.mustNotCall());

  await new Promise((resolve) => {
    stream.on('end', common.mustCall(resolve));
    stream.write(Buffer.from('foo'));
  });
});

test('SyncTransform - returning null ends the stream deferred', async () => {
  const stream = SyncTransform(function() {
    return null;
  });

  stream.on('data', common.mustNotCall());

  await new Promise((resolve) => {
    stream.on('end', common.mustCall(resolve));

    setImmediate(() => {
      stream.write(Buffer.from('foo'));
    });
  });
});

test('SyncTransform - returning null ends the stream when piped', async () => {
  const stream = SyncTransform(function() {
    return null;
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);

  await new Promise((resolve) => {
    const sink = stringSink([]);
    sink.on('finish', common.mustCall(resolve));
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - support flush', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  }, function() {
    return Buffer.from('done!');
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected = [Buffer.from('FOO'), Buffer.from('BAR'), Buffer.from('done!')];

  await new Promise((resolve) => {
    const sink = stringSink(expected, resolve);
    sink.on('finish', common.mustCall());
    from.pipe(stream).pipe(sink);
  });
});

test('SyncTransform - adding on(\'data\') after pipe throws', () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });

  const sink = new Writable();

  stream.pipe(sink);

  assert.throws(() => {
    stream.on('data', () => {});
  }, /you can use only pipe\(\) or on\('data'\)/);
});

test('SyncTransform - multiple data event', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const from = stringFrom([Buffer.from('foo'), Buffer.from('bar')]);
  const expected1 = [Buffer.from('FOO'), Buffer.from('BAR')];
  const expected2 = [Buffer.from('FOO'), Buffer.from('BAR')];

  await new Promise((resolve) => {
    let count = 0;
    const done = () => {
      count++;
      if (count === 4) resolve();
    };

    stream.on('data', (chunk) => {
      assert.strictEqual(chunk.toString(), expected1.shift().toString());
      done();
    });

    stream.on('data', (chunk) => {
      assert.strictEqual(chunk.toString(), expected2.shift().toString());
      done();
    });

    from.pipe(stream);
  });
});

test('SyncTransform - piping twice errors', () => {
  const stream = SyncTransform();
  stream.pipe(new Writable());

  assert.throws(() => {
    stream.pipe(new Writable());
  }, /multiple pipe not allowed/);
});

test('SyncTransform - removing on(\'data\') handlers', async () => {
  const stream = SyncTransform(function(chunk) {
    return Buffer.from(chunk.toString().toUpperCase());
  });
  const expected = [Buffer.from('FOO'), Buffer.from('BAR')];

  function first(chunk) {
    assert.strictEqual(chunk.toString(), expected.shift().toString());
  }

  function second() {
    assert.fail('should never be called');
  }

  stream.on('data', first);
  stream.on('data', second);

  stream.removeListener('data', second);

  await new Promise((resolve) => {
    stream.write('foo');

    stream.once('drain', () => {
      stream.removeListener('data', first);
      stream.on('data', first);
      stream.write('bar');
      resolve();
    });
  });
});

test('SyncTransform - double unpipe does nothing', () => {
  const stream = SyncTransform();
  const dest = new Writable();

  stream.pipe(dest);
  stream.unpipe(dest);
  stream.unpipe(dest);

  stream.write('hello');
});

test('SyncTransform - must respect backpressure', async () => {
  const stream = SyncTransform();

  assert.strictEqual(stream.write('hello'), false);

  await new Promise((resolve) => {
    stream.once('error', common.mustCall(() => {
      resolve();
    }));

    assert.strictEqual(stream.write('world'), false);
  });
});

test('SyncTransform - works with pipeline and calls flush', async () => {
  const expected = 'hello world!';
  let actual = '';

  await new Promise((resolve, reject) => {
    pipeline(
      Readable.from('hello world'),
      SyncTransform(
        undefined,
        function flush() {
          this.push('!');
        }
      ),
      new Writable({
        write(chunk, enc, cb) {
          actual += chunk.toString();
          cb();
        },
      }),
      (err) => {
        if (err) return reject(err);
        assert.strictEqual(actual, expected);
        resolve();
      }
    );
  });
});

test('SyncTransform - works with pipeline and calls flush / 2', async () => {
  const expected = 'hello world!';
  let actual = '';

  await new Promise((resolve, reject) => {
    pipeline(
      Readable.from('hello world'),
      SyncTransform(
        undefined,
        function flush() {
          return '!';
        }
      ),
      new Writable({
        write(chunk, enc, cb) {
          actual += chunk.toString();
          cb();
        },
      }),
      (err) => {
        if (err) return reject(err);
        assert.strictEqual(actual, expected);
        resolve();
      }
    );
  });
});

test('SyncTransform - can be called without new', () => {
  const stream = SyncTransform();
  assert.ok(stream instanceof SyncTransform);
});

test('SyncTransform - validates transform function', () => {
  assert.throws(() => {
    SyncTransform('not a function');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

test('SyncTransform - validates flush function', () => {
  assert.throws(() => {
    SyncTransform(null, 'not a function');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
