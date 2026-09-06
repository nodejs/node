'use strict';

// Tests that StreamBase::Writev correctly handles string chunks by verifying
// data integrity when string chunks, buffer chunks, and mixed chunks are
// written via cork/uncork. This exercises the chunk-cache path (all_buffers=false)
// introduced to avoid redundant V8 array accesses between the sizing and write
// passes.

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

function collectWritev(decodeStrings) {
  const received = [];
  const w = new Writable({
    decodeStrings,
    writev(chunks, cb) {
      for (const { chunk, encoding } of chunks)
        received.push({ chunk, encoding });
      cb();
    },
    write(chunk, encoding, cb) {
      received.push({ chunk, encoding });
      cb();
    },
  });
  return { w, received };
}

// String chunks only — various encodings, decodeStrings: false so we can
// inspect the raw string/encoding pairs the stream layer received.
{
  const { w, received } = collectWritev(false);
  w.cork();
  w.write('hello', 'utf8');
  w.write('world', 'latin1');
  w.write('cafebabe', 'hex');
  w.uncork();
  w.end();

  w.on('finish', common.mustCall(() => {
    assert.strictEqual(received[0].chunk, 'hello');
    assert.strictEqual(received[0].encoding, 'utf8');
    assert.strictEqual(received[1].chunk, 'world');
    assert.strictEqual(received[1].encoding, 'latin1');
    assert.strictEqual(received[2].chunk, 'cafebabe');
    assert.strictEqual(received[2].encoding, 'hex');
  }));
}

// Mixed buffer + string chunks — the non-all_buffers branch in Writev.
{
  const { w, received } = collectWritev(false);
  const buf = Buffer.from('buffered');
  w.cork();
  w.write(buf);
  w.write('stringy', 'utf8');
  w.write(buf);
  w.uncork();
  w.end();

  w.on('finish', common.mustCall(() => {
    assert(Buffer.isBuffer(received[0].chunk));
    assert.strictEqual(received[0].chunk.toString(), 'buffered');
    assert.strictEqual(received[1].chunk, 'stringy');
    assert.strictEqual(received[1].encoding, 'utf8');
    assert(Buffer.isBuffer(received[2].chunk));
    assert.strictEqual(received[2].chunk.toString(), 'buffered');
  }));
}

// More than 16 chunks so MaybeStackBuffer<CachedChunk, 16> must heap-allocate.
// Verify that every string arrives correctly after the realloc.
{
  const COUNT = 20;
  const chunks = [];
  const { w, received } = collectWritev(false);
  w.cork();
  for (let i = 0; i < COUNT; i++) {
    const s = `chunk-${i}`;
    chunks.push(s);
    w.write(s, 'utf8');
  }
  w.uncork();
  w.end();

  w.on('finish', common.mustCall(() => {
    assert.strictEqual(received.length, COUNT);
    for (let i = 0; i < COUNT; i++) {
      assert.strictEqual(received[i].chunk, chunks[i]);
      assert.strictEqual(received[i].encoding, 'utf8');
    }
  }));
}

// decodeStrings: true — all strings should arrive as Buffers with correct data.
{
  const { w, received } = collectWritev(true);
  w.cork();
  w.write('café', 'utf8');
  w.write(Buffer.from('raw'));
  w.uncork();
  w.end();

  w.on('finish', common.mustCall(() => {
    assert(Buffer.isBuffer(received[0].chunk));
    assert.strictEqual(received[0].chunk.toString('utf8'), 'café');
    assert(Buffer.isBuffer(received[1].chunk));
    assert.strictEqual(received[1].chunk.toString(), 'raw');
  }));
}
