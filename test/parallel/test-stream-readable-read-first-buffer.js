'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

// Test that read() without arguments always returns only the first buffered
// chunk rather than concatenating all buffered data. This applies regardless
// of whether the stream is flowing or paused.

{
  // Paused mode: read() should return first chunk only.
  const r = new Readable({
    read() {},
  });

  r.push('a');
  r.push('b');
  r.push('c');

  r.on('readable', common.mustCallAtLeast(() => {
    const chunk = r.read();
    if (chunk === null) return;
    assert.strictEqual(chunk.toString(), 'a');
    assert.strictEqual(r.read().toString(), 'b');
    assert.strictEqual(r.read().toString(), 'c');
    assert.strictEqual(r.read(), null);
    r.push(null);
  }, 1));

  r.on('end', common.mustCall());
}

{
  // Paused mode with Buffer chunks: read() should return first buffer only.
  const r = new Readable({
    read() {},
  });

  r.push(Buffer.from('hello'));
  r.push(Buffer.from('world'));

  r.on('readable', common.mustCallAtLeast(() => {
    const chunk = r.read();
    if (chunk === null) return;
    assert.strictEqual(chunk.toString(), 'hello');
    assert.strictEqual(r.read().toString(), 'world');
    assert.strictEqual(r.read(), null);
    r.push(null);
  }, 1));

  r.on('end', common.mustCall());
}

{
  // Flowing mode: each 'data' event should emit one chunk at a time.
  const r = new Readable({
    read() {},
  });

  const chunks = [];
  r.on('data', (chunk) => {
    chunks.push(chunk.toString());
  });

  r.on('end', common.mustCall(() => {
    assert.deepStrictEqual(chunks, ['x', 'y', 'z']);
  }));

  r.push('x');
  r.push('y');
  r.push('z');
  r.push(null);
}
