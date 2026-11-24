// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  arrayBuffer,
  blob,
  buffer,
  bytes,
  text,
  json,
} = require('stream/consumers');

const {
  Readable,
  PassThrough
} = require('stream');

const {
  TransformStream,
} = require('stream/web');

const buf = Buffer.from('hellothere');
const kArrayBuffer =
  buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);

{
  const passthrough = new PassThrough();

  blob(passthrough).then(async (blob) => {
    assert.strictEqual(blob.size, 10);
    assert.deepStrictEqual(await blob.arrayBuffer(), kArrayBuffer);
  }).then(common.mustCall());

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  arrayBuffer(passthrough).then(common.mustCall((ab) => {
    assert.strictEqual(ab.byteLength, 10);
    assert.deepStrictEqual(ab, kArrayBuffer);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  buffer(passthrough).then(common.mustCall((buf) => {
    assert.strictEqual(buf.byteLength, 10);
    assert.deepStrictEqual(buf.buffer, kArrayBuffer);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  bytes(passthrough).then(common.mustCall((uint8arr) => {
    assert(uint8arr instanceof Uint8Array);
    assert.strictEqual(uint8arr.byteLength, 10);
    assert.deepStrictEqual(Buffer.from(uint8arr), buf);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  text(passthrough).then(common.mustCall((str) => {
    assert.strictEqual(str.length, 10);
    assert.strictEqual(str, 'hellothere');
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const readable = new Readable({
    read() {}
  });

  text(readable).then((data) => {
    assert.strictEqual(data, 'foo\ufffd\ufffd\ufffd');
  }).then(common.mustCall());

  readable.push(new Uint8Array([0x66, 0x6f, 0x6f, 0xed, 0xa0, 0x80]));
  readable.push(null);
}

{
  const passthrough = new PassThrough();

  json(passthrough).then(common.mustCall((str) => {
    assert.strictEqual(str.length, 10);
    assert.strictEqual(str, 'hellothere');
  }));

  passthrough.write('"hello');
  setTimeout(() => passthrough.end('there"'), 10);
}

{
  const { writable, readable } = new TransformStream();

  blob(readable).then(async (blob) => {
    assert.strictEqual(blob.size, 10);
    assert.deepStrictEqual(await blob.arrayBuffer(), kArrayBuffer);
  }).then(common.mustCall());

  const writer = writable.getWriter();
  writer.write('hello');
  setTimeout(() => {
    writer.write('there');
    writer.close();
  }, 10);

  assert.rejects(blob(readable), { code: 'ERR_INVALID_STATE' }).then(common.mustCall());
}

{
  const { writable, readable } = new TransformStream();

  arrayBuffer(readable).then(common.mustCall((ab) => {
    assert.strictEqual(ab.byteLength, 10);
    assert.deepStrictEqual(ab, kArrayBuffer);
  }));

  const writer = writable.getWriter();
  writer.write('hello');
  setTimeout(() => {
    writer.write('there');
    writer.close();
  }, 10);

  assert.rejects(arrayBuffer(readable), { code: 'ERR_INVALID_STATE' }).then(common.mustCall());
}

{
  const { writable, readable } = new TransformStream();

  text(readable).then(common.mustCall((str) => {
    assert.strictEqual(str.length, 10);
    assert.strictEqual(str, 'hellothere');
  }));

  const writer = writable.getWriter();
  writer.write('hello');
  setTimeout(() => {
    writer.write('there');
    writer.close();
  }, 10);

  assert.rejects(text(readable), { code: 'ERR_INVALID_STATE' }).then(common.mustCall());
}

{
  const { writable, readable } = new TransformStream();

  json(readable).then(common.mustCall((str) => {
    assert.strictEqual(str.length, 10);
    assert.strictEqual(str, 'hellothere');
  }));

  const writer = writable.getWriter();
  writer.write('"hello');
  setTimeout(() => {
    writer.write('there"');
    writer.close();
  }, 10);

  assert.rejects(json(readable), { code: 'ERR_INVALID_STATE' }).then(common.mustCall());
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  blob(stream).then(common.mustCall((blob) => {
    assert.strictEqual(blob.size, 30);
  }));

  stream.write({});
  stream.end({});
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  arrayBuffer(stream).then(common.mustCall((ab) => {
    assert.strictEqual(ab.byteLength, 30);
    assert.strictEqual(
      Buffer.from(ab).toString(),
      '[object Object][object Object]');
  }));

  stream.write({});
  stream.end({});
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  buffer(stream).then(common.mustCall((buf) => {
    assert.strictEqual(buf.byteLength, 30);
    assert.strictEqual(
      buf.toString(),
      '[object Object][object Object]');
  }));

  stream.write({});
  stream.end({});
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  bytes(stream).then(common.mustCall((uint8arr) => {
    assert(uint8arr instanceof Uint8Array);
    assert.strictEqual(uint8arr.byteLength, 30);
    assert.strictEqual(
      Buffer.from(uint8arr).toString(),
      '[object Object][object Object]');
  }));

  stream.write({});
  stream.end({});
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  assert.rejects(text(stream), {
    code: 'ERR_INVALID_ARG_TYPE',
  }).then(common.mustCall());

  stream.write({});
  stream.end({});
}

{
  const stream = new PassThrough({
    readableObjectMode: true,
    writableObjectMode: true,
  });

  assert.rejects(json(stream), {
    code: 'ERR_INVALID_ARG_TYPE',
  }).then(common.mustCall());

  stream.write({});
  stream.end({});
}

{
  const stream = new TransformStream();
  text(stream.readable).then(common.mustCall((str) => {
    // Incomplete utf8 character is flushed as a replacement char
    assert.strictEqual(str.charCodeAt(0), 0xfffd);
  }));
  const writer = stream.writable.getWriter();
  Promise.all([
    writer.write(new Uint8Array([0xe2])),
    writer.write(new Uint8Array([0x82])),
    writer.close(),
  ]).then(common.mustCall());
}

{
  arrayBuffer(Readable.from([])).then(common.mustCall((ab) => {
    assert(ab instanceof ArrayBuffer);
    assert.strictEqual(ab.byteLength, 0);
  }));
}

{
  const a = new Uint8Array([1, 2, 3]);
  const b = new Uint8Array([4, 5]);
  arrayBuffer(Readable.from([a, new Uint8Array(0), b])).then(common.mustCall((ab) => {
    a.fill(0);
    b.fill(0);
    assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([1, 2, 3, 4, 5]));
  }));
}

{
  const detached = new ArrayBuffer(4);
  const detachedView = new Uint8Array(detached);
  detachedView.set([9, 8, 7, 6]);
  detached.transfer();
  arrayBuffer(Readable.from([new Uint8Array([1, 2]), detachedView, new Uint8Array([3])]))
    .then(common.mustCall((ab) => {
      assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([1, 2, 3]));
    }));
}

{
  const resizable = new ArrayBuffer(4, { maxByteLength: 16 });
  assert.rejects(
    arrayBuffer(Readable.from([Buffer.from('a'), new Uint8Array(resizable)])),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: /resizable ArrayBuffer/,
    },
  ).then(common.mustCall());
}

{
  const sab = new SharedArrayBuffer(4);
  assert.rejects(
    arrayBuffer(Readable.from([Buffer.from('a'), new Uint8Array(sab)])),
    {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: /SharedArrayBuffer/,
    },
  ).then(common.mustCall());
}

{
  buffer(Readable.from([Buffer.from('hello '), 'world'])).then(common.mustCall((buf) => {
    assert.strictEqual(buf.toString(), 'hello world');
  }));
}

{
  const u16 = new Uint16Array([0x0201]);
  const dvBuf = new ArrayBuffer(4);
  new Uint8Array(dvBuf).set([10, 20, 30, 40]);
  const rawAb = new Uint8Array([50, 60]).buffer;
  const clamped = new Uint8ClampedArray([70, 80]);
  const mixed = [
    Buffer.from([0]),
    u16,
    new DataView(dvBuf, 1, 2),
    rawAb,
    clamped,
  ];
  const expected = Buffer.concat([
    Buffer.from([0]),
    Buffer.from(u16.buffer, u16.byteOffset, u16.byteLength),
    Buffer.from([20, 30, 50, 60, 70, 80]),
  ]);
  buffer(Readable.from(mixed)).then(common.mustCall((buf) => {
    assert.deepStrictEqual(buf, expected);
  }));
}
