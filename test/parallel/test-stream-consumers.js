// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  arrayBuffer,
  blob,
  buffer,
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

  blob(passthrough).then(common.mustCall(async (blob) => {
    assert.strictEqual(blob.size, 10);
    assert.deepStrictEqual(await blob.arrayBuffer(), kArrayBuffer);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  arrayBuffer(passthrough).then(common.mustCall(async (ab) => {
    assert.strictEqual(ab.byteLength, 10);
    assert.deepStrictEqual(ab, kArrayBuffer);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}

{
  const passthrough = new PassThrough();

  buffer(passthrough).then(common.mustCall(async (buf) => {
    assert.strictEqual(buf.byteLength, 10);
    assert.deepStrictEqual(buf.buffer, kArrayBuffer);
  }));

  passthrough.write('hello');
  setTimeout(() => passthrough.end('there'), 10);
}


{
  const passthrough = new PassThrough();

  text(passthrough).then(common.mustCall(async (str) => {
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
  });

  readable.push(new Uint8Array([0x66, 0x6f, 0x6f, 0xed, 0xa0, 0x80]));
  readable.push(null);
}

{
  const passthrough = new PassThrough();

  json(passthrough).then(common.mustCall(async (str) => {
    assert.strictEqual(str.length, 10);
    assert.strictEqual(str, 'hellothere');
  }));

  passthrough.write('"hello');
  setTimeout(() => passthrough.end('there"'), 10);
}

{
  const { writable, readable } = new TransformStream();

  blob(readable).then(common.mustCall(async (blob) => {
    assert.strictEqual(blob.size, 10);
    assert.deepStrictEqual(await blob.arrayBuffer(), kArrayBuffer);
  }));

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

  arrayBuffer(readable).then(common.mustCall(async (ab) => {
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

  text(readable).then(common.mustCall(async (str) => {
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

  json(readable).then(common.mustCall(async (str) => {
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
