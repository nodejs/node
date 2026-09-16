'use strict';

require('../common');
const assert = require('assert');
const {
  ReadableStream,
  WritableStream,
  TransformStream,
} = require('stream/web');

// Streams created by internal code paths (transform stream sides, tee
// branches, ReadableStream.from, transferred streams) are plain
// ReadableStream/WritableStream instances: same prototype and no own
// properties beyond what the public constructors create.

function check(stream, Class) {
  assert.ok(stream instanceof Class);
  assert.strictEqual(Object.getPrototypeOf(stream), Class.prototype);
  assert.strictEqual(stream.constructor, Class);
  assert.deepStrictEqual(Object.keys(stream), Object.keys(new Class()));
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(stream, 'constructor'), undefined);
}

{
  const { readable, writable } = new TransformStream();
  check(readable, ReadableStream);
  check(writable, WritableStream);
}

{
  const [branch1, branch2] = new ReadableStream().tee();
  check(branch1, ReadableStream);
  check(branch2, ReadableStream);
}

{
  const [branch1, branch2] = new ReadableStream({ type: 'bytes' }).tee();
  check(branch1, ReadableStream);
  check(branch2, ReadableStream);
}

check(ReadableStream.from([]), ReadableStream);

{
  const readable = new ReadableStream();
  const writable = new WritableStream();
  const transferred = structuredClone(
    { readable, writable },
    { transfer: [readable, writable] });
  check(transferred.readable, ReadableStream);
  check(transferred.writable, WritableStream);
}
