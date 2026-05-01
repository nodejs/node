'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const test = require('node:test');
const { ReadableStream } = require('stream/web');

const sab = new SharedArrayBuffer(8);
const sabView = new Uint8Array(sab);
const sabDataView = new DataView(sab);

// -- ReadableStreamBYOBReader.read() --

test('ReadableStreamBYOBReader.read() rejects SAB-backed Uint8Array', async () => {
  const rs = new ReadableStream({
    type: 'bytes',
    pull(controller) {
      controller.enqueue(new Uint8Array([1, 2, 3]));
    },
  });
  const reader = rs.getReader({ mode: 'byob' });
  await assert.rejects(
    reader.read(new Uint8Array(sab)),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  reader.releaseLock();
});

test('ReadableStreamBYOBReader.read() rejects SAB-backed DataView', async () => {
  const rs = new ReadableStream({
    type: 'bytes',
    pull(controller) {
      controller.enqueue(new Uint8Array([1, 2, 3]));
    },
  });
  const reader = rs.getReader({ mode: 'byob' });
  await assert.rejects(
    reader.read(sabDataView),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
  reader.releaseLock();
});

test('ReadableStreamBYOBReader.read() accepts regular view', async () => {
  const rs = new ReadableStream({
    type: 'bytes',
    pull(controller) {
      controller.enqueue(new Uint8Array([1, 2, 3]));
    },
  });
  const reader = rs.getReader({ mode: 'byob' });
  const { value, done } = await reader.read(new Uint8Array(3));
  assert.strictEqual(done, false);
  assert.deepStrictEqual(value, new Uint8Array([1, 2, 3]));
  reader.releaseLock();
});

// -- ReadableByteStreamController.enqueue() --

test('ReadableByteStreamController.enqueue() rejects SAB-backed Uint8Array', async () => {
  const sabForEnqueue = new SharedArrayBuffer(4);
  const sabViewForEnqueue = new Uint8Array(sabForEnqueue);
  sabViewForEnqueue[0] = 42;

  const rs = new ReadableStream({
    type: 'bytes',
    pull: common.mustCall((controller) => {
      assert.throws(
        () => controller.enqueue(sabViewForEnqueue),
        { code: 'ERR_INVALID_ARG_VALUE' },
      );
      controller.enqueue(new Uint8Array([1]));
    }),
  });
  const reader = rs.getReader();
  const { value } = await reader.read();
  assert.deepStrictEqual(value, new Uint8Array([1]));
  reader.releaseLock();
});

test('ReadableByteStreamController.enqueue() rejects SAB-backed DataView', async () => {
  const sabForDv = new SharedArrayBuffer(4);
  const dvForEnqueue = new DataView(sabForDv);

  const rs = new ReadableStream({
    type: 'bytes',
    pull: common.mustCall((controller) => {
      assert.throws(
        () => controller.enqueue(dvForEnqueue),
        { code: 'ERR_INVALID_ARG_VALUE' },
      );
      controller.enqueue(new Uint8Array([2]));
    }),
  });
  const reader = rs.getReader();
  const { value } = await reader.read();
  assert.deepStrictEqual(value, new Uint8Array([2]));
  reader.releaseLock();
});

// -- SharedWebIDL converters --

const { converters } = require('internal/webidl');

test('webidl converters.BufferSource rejects SharedArrayBuffer', () => {
  assert.throws(
    () => converters.BufferSource(sab),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.BufferSource rejects SAB-backed Uint8Array', () => {
  assert.throws(
    () => converters.BufferSource(sabView),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.BufferSource rejects SAB-backed DataView', () => {
  assert.throws(
    () => converters.BufferSource(sabDataView),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.BufferSource accepts ArrayBuffer', () => {
  const ab = new ArrayBuffer(4);
  assert.strictEqual(converters.BufferSource(ab), ab);
});

test('webidl converters.BufferSource accepts regular TypedArray', () => {
  const ta = new Uint8Array(4);
  assert.strictEqual(converters.BufferSource(ta), ta);
});

test('webidl converters.ArrayBufferView rejects SAB-backed Uint8Array', () => {
  assert.throws(
    () => converters.ArrayBufferView(sabView),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.ArrayBufferView rejects SAB-backed DataView', () => {
  assert.throws(
    () => converters.ArrayBufferView(sabDataView),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.ArrayBufferView rejects non-view', () => {
  assert.throws(
    () => converters.ArrayBufferView('not a view'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
});

test('webidl converters.ArrayBufferView accepts regular Uint8Array', () => {
  const ta = new Uint8Array(4);
  assert.strictEqual(converters.ArrayBufferView(ta), ta);
});

test('webidl converters.ArrayBufferView accepts regular DataView', () => {
  const dv = new DataView(new ArrayBuffer(4));
  assert.strictEqual(converters.ArrayBufferView(dv), dv);
});
