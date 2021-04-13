// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

// Tests the functionality of the quic.EndpointConfig object, ensuring
// that validation of all of the properties is as expected.

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');

const {
  Buffer,
  Blob,
} = require('buffer');

const {
  internalBinding,
} = require('internal/test/binding');

const {
  ArrayBufferViewSource,
  BlobSource,
  StreamSource,
} = internalBinding('quic');

const {
  acquireBody,
} = require('internal/quic/common');

const {
  setTimeout: wait,
} = require('timers/promises');

const kBlob = new Blob();

const {
  ReadableStream,
} = require('stream/web');

(async () => {
  assert.strictEqual(await acquireBody(), undefined);
  assert.strictEqual(await acquireBody(null), undefined);

  [
    'hello',
    new ArrayBuffer(10),
    new Uint8Array(10),
    new Uint32Array(10),
    new BigInt64Array(10),
    new DataView(new ArrayBuffer(10)),
    Buffer.alloc(10),
    () => 'hello',
    () => new ArrayBuffer(10),
    () => new Uint8Array(10),
    () => new Uint32Array(10),
    () => new BigInt64Array(10),
    () => new DataView(new ArrayBuffer(10)),
    () => Buffer.alloc(10),
    async () => 'hello',
    async () => new ArrayBuffer(10),
    async () => new Uint8Array(10),
    async () => new Uint32Array(10),
    async () => new BigInt64Array(10),
    async () => new DataView(new ArrayBuffer(10)),
    async () => Buffer.alloc(10),
    async () => { await wait(10); return 'hello'; },
    async () => { await wait(10); return new ArrayBuffer(10); },
    async () => { await wait(10); return new Uint8Array(10); },
    async () => { await wait(10); return new Uint32Array(10); },
    async () => { await wait(10); return new BigInt64Array(10); },
    async () => { await wait(10); return new DataView(new ArrayBuffer(10)); },
    async () => { await wait(10); return Buffer.alloc(10); },
    Promise.resolve('hello'),
  ].forEach(async (i) => {
    const body = await acquireBody(i);
    assert(body instanceof ArrayBufferViewSource);
  });

  [
    kBlob,
    () => kBlob,
    async () => kBlob,
    async () => { await wait(10); return kBlob; },
    Promise.resolve(kBlob),
  ].forEach(async (i) => {
    const body = await acquireBody(i);
    assert(body instanceof BlobSource);
  });

  // Can't test these two options here because they
  // require a live stream to be associated.
  // [
  //   await open(__filename, 'r'),
  //   open(__filename, 'r'),
  // ].forEach(async (i) => {
  //   const body = await acquireBody(i);
  //   assert(body instanceof StreamBaseSource);
  // });

  // [
  //   createReadStream(__filename),
  // ].forEach(async (i) => {
  //   const body = await acquireBody(i);
  //   assert(body instanceof StreamSource);
  // });

  [
    new ReadableStream(),
    () => new ReadableStream(),
    async () => new ReadableStream(),
    Promise.resolve(new ReadableStream()),
  ].forEach(async (i) => {
    const body = await acquireBody(i);
    assert(body instanceof StreamSource);
  });

  [1, false, {}].forEach((i) => {
    assert.rejects(acquireBody(i), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
})().then(common.mustCall());
