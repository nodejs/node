// Flags: --js-immutable-arraybuffer --experimental-stream-iter
'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const { Readable } = require('node:stream');
const iter = require('node:stream/iter');
const { serialize, deserialize } = require('node:v8');

// V8 currently checks the source of TypedArray.prototype.set() for write
// access. Copying immutable bytes into a mutable destination must be allowed.
function immutable(bytes = [65, 66, 67, 68]) {
  return new Uint8Array(Uint8Array.from(bytes).buffer.transferToImmutable());
}

for (const [name, copy] of [
  ['Buffer.from', (view) => Buffer.from(view)],
  ['Buffer.copyBytesFrom', (view) => Buffer.copyBytesFrom(view)],
  ['Buffer.concat', (view) => Buffer.concat([view])],
  ['Buffer.concat with length', (view) => Buffer.concat([view], view.length)],
]) {
  test(name, () => {
    const source = immutable();
    assert.deepStrictEqual([...copy(source)], [...source]);
  });
}

test('TextDecoder single-byte non-ASCII input', () => {
  assert.strictEqual(new TextDecoder('windows-1252').decode(immutable([0xe9])), '\u00e9');
});

test('TextDecoder split UTF-8 sequence', () => {
  const decoder = new TextDecoder();
  assert.strictEqual(decoder.decode(immutable([0xc3]), { stream: true }), '');
  assert.strictEqual(decoder.decode(immutable([0xa9])), '\u00e9');
});

test('TextDecoderStream split UTF-8 sequence', async () => {
  const source = new ReadableStream({
    start(controller) {
      controller.enqueue(immutable([0xc3]));
      controller.enqueue(immutable([0xa9]));
      controller.close();
    },
  });
  let result = '';
  for await (const chunk of source.pipeThrough(new TextDecoderStream())) {
    result += chunk;
  }
  assert.strictEqual(result, '\u00e9');
});

test('Readable.read spanning chunks', (t) => {
  const stream = new Readable({ read() {} });
  t.after(() => stream.destroy());
  stream.push(Buffer.from(immutable().buffer));
  stream.push(Buffer.from(immutable().buffer));
  stream.push(null);
  assert.deepStrictEqual([...stream.read(6)], [65, 66, 67, 68, 65, 66]);
});

for (const name of ['bytes', 'bytesSync', 'arrayBuffer', 'arrayBufferSync', 'text', 'textSync']) {
  test(`stream/iter.${name}`, async () => {
    // A partial view cannot use the full-buffer fast path.
    const result = await iter[name](immutable().subarray(1));
    if (typeof result === 'string') {
      assert.strictEqual(result, 'BCD');
    } else {
      assert.deepStrictEqual([...new Uint8Array(result)], [66, 67, 68]);
    }
  });
}

for (const [name, consume] of [
  ['Response', (view) => new Response(view).text()],
  ['Request', (view) => new Request('http://localhost/', {
    method: 'POST', body: view,
  }).text()],
]) {
  test(`${name} body`, async () => {
    assert.strictEqual(await consume(immutable()), 'ABCD');
  });
}

test('Response consumes immutable stream chunks', async () => {
  const stream = new ReadableStream({
    start(controller) {
      controller.enqueue(immutable());
      controller.close();
    },
  });
  assert.strictEqual(await new Response(stream).text(), 'ABCD');
});

test('v8.deserialize copies an unaligned payload from immutable storage', () => {
  const wire = serialize(new Uint32Array([1, 2]));
  const source = wire.buffer.sliceToImmutable(wire.byteOffset, wire.byteOffset + wire.length);
  assert.deepStrictEqual([...deserialize(new Uint8Array(source))], [1, 2]);
});
