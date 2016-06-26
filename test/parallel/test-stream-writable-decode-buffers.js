'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');

class ChunkStoringWritable extends stream.Writable {
  constructor(options) {
    super(options);

    this.chunks = [];
  }

  _write(data, encoding, callback) {
    this.chunks.push({ data, encoding });
    callback();
  }
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });
  w.write(Buffer.from('e4bd', 'hex'));
  w.write(Buffer.from('a0e5', 'hex'));
  w.write(Buffer.from('a5bd', 'hex'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['', '你', '好']);
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });
  w.write(Buffer.from('你', 'utf8'));
  w.write(Buffer.from('好', 'utf8'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['你', '好']);
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });
  w.write(Buffer.from('80', 'hex'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['\ufffd']);
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });
  w.write(Buffer.from('c3', 'hex'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['', '\ufffd']);
}

{
  const w = new ChunkStoringWritable({
    decodeBuffers: true,
    defaultEncoding: 'utf16le'
  });

  w.write(Buffer.from('你好', 'utf16le'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['你好']);
}

{
  const w = new ChunkStoringWritable({
    decodeBuffers: true,
    defaultEncoding: 'base64'
  });

  w.write(Buffer.from('你好', 'utf16le'));
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['YE99', 'WQ==']);
}

{
  const w = new ChunkStoringWritable({
    decodeBuffers: true,
    defaultEncoding: 'utf16le'
  });

  w.write('你好', 'utf16le');
  w.end();

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['你好']);
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });

  w.write(Buffer.from([0x44, 0xc3])); // Ends on incomplete UTF8.

  // This write should *not* be passed through directly as there's
  // input pending.
  w.write('a');

  w.end(Buffer.from('bc7373656c', 'hex'));

  assert.deepStrictEqual(w.chunks.map((c) => c.data), ['D', '�a', '�ssel']);
}

{
  const w = new ChunkStoringWritable({ decodeBuffers: true });

  w.write('a');
  w.setDefaultEncoding('ucs2');
  w.end('换');

  assert.deepStrictEqual(w.chunks, [
    { data: 'a', encoding: 'utf8' },
    { data: 'bc', encoding: 'utf8' }
  ]);
}

assert.throws(() => new stream.Writable({
  decodeBuffers: true,
  decodeStrings: true
}), /decodeBuffers and decodeStrings cannot both be true/);
