'use strict';
const common = require('../common');
const assert = require('assert');
const { PassThrough, Writable } = require('stream');
const { test } = require('node:test');
const { isDataView } = require('util').types;
const vm = require('vm');

const typedArrayPrototype = Object.getPrototypeOf(Uint8Array.prototype);
const typedArrayGetters = {};
const dataViewGetters = {};
for (const key of ['buffer', 'byteLength', 'byteOffset']) {
  typedArrayGetters[key] = Function.call.bind(
    Object.getOwnPropertyDescriptor(typedArrayPrototype, key).get);
  dataViewGetters[key] = Function.call.bind(
    Object.getOwnPropertyDescriptor(DataView.prototype, key).get);
}

function metadata(view, key) {
  return (isDataView(view) ? dataViewGetters : typedArrayGetters)[key](view);
}

function bytes(view) {
  return new Uint8Array(
    metadata(view, 'buffer'),
    metadata(view, 'byteOffset'),
    metadata(view, 'byteLength'),
  );
}

function assertInitializedCopy(view, expected) {
  const buffer = metadata(view, 'buffer');
  assert.strictEqual(metadata(view, 'byteLength'), expected.length);
  const initialized = new Uint8Array(buffer.byteLength);
  initialized.set(expected, metadata(view, 'byteOffset'));
  assert.deepStrictEqual(new Uint8Array(buffer), initialized);
}

async function capture(input) {
  let copied;
  const writable = new Writable({ write: common.mustNotCall() });
  writable.write = common.mustCall((chunk) => {
    copied = chunk;
    return true;
  });
  writable.on('error', common.mustNotCall());
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(input);
  await writer.close();
  return copied;
}

for (const size of [0, 64, 65]) {
  test(`Buffer copy has independent, fully initialized backing: ${size} bytes`, async () => {
    const input = Buffer.allocUnsafe(size + 32).subarray(16, size + 16);
    for (let i = 0; i < size; i++) input[i] = i % 251;
    const expected = Array.from(input);
    const copied = await capture(input);

    assert(Buffer.isBuffer(copied));
    assert.notStrictEqual(copied, input);
    assert.notStrictEqual(copied.buffer, input.buffer);
    assertInitializedCopy(copied, expected);
    new Uint8Array(input.buffer).fill(255);
    assert.deepStrictEqual(Array.from(copied), expected);
  });
}

const constructors = [
  Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array,
  Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array,
  BigInt64Array, BigUint64Array, DataView, Buffer,
];
for (const Backing of [ArrayBuffer, SharedArrayBuffer]) {
  for (const Constructor of constructors) {
    test(`${Backing.name} copy preserves ${Constructor.name} and raw bytes`, async () => {
      const buffer = new Backing(64);
      const input = Constructor === Buffer ? Buffer.from(buffer, 8, 16) :
        Constructor === DataView ? new DataView(buffer, 8, 16) :
          new Constructor(buffer, 8, 16 / Constructor.BYTES_PER_ELEMENT);
      const expected = Array.from({ length: 16 }, (_, i) => i + 1);
      new Uint8Array(buffer, 8, 16).set(expected);
      const copied = await capture(input);

      new Uint8Array(buffer).fill(255);
      assert.notStrictEqual(metadata(copied, 'buffer'), buffer);
      assertInitializedCopy(copied, expected);
      assert.strictEqual(metadata(copied, 'buffer') instanceof Backing, true);
      assert.strictEqual(Buffer.isBuffer(copied), Constructor === Buffer);
      assert.strictEqual(Object.prototype.toString.call(copied), Object.prototype.toString.call(input));
      assert.deepStrictEqual(Array.from(bytes(copied)), expected);
    });
  }
}

for (const Constructor of [Uint8Array, Buffer]) {
  for (const length of [0, 8]) {
    test(`out-of-bounds resizable ${Constructor.name} remains an empty copy: ${length}`, async () => {
      const buffer = new ArrayBuffer(32, { maxByteLength: 64 });
      const input = Constructor === Buffer ? Buffer.from(buffer, 16, length) :
        new Uint8Array(buffer, 16, length);
      buffer.resize(8);
      const copied = await capture(input);

      assert.strictEqual(copied.byteLength, 0);
      assert.notStrictEqual(copied.buffer, buffer);
      assert.strictEqual(Buffer.isBuffer(copied), Constructor === Buffer);
    });
  }
}

for (const Backing of [ArrayBuffer, SharedArrayBuffer]) {
  for (const tracking of [false, true]) {
    test(`${Backing.name} copy survives resize or grow, tracking=${tracking}`, async () => {
      const buffer = new Backing(32, { maxByteLength: 64 });
      const input = tracking ? new Uint8Array(buffer, 8) : new Uint8Array(buffer, 8, 16);
      input.fill(7);
      const expected = Array.from(input);
      const copied = await capture(input);

      if (Backing === ArrayBuffer) buffer.resize(8);
      else buffer.grow(64);
      new Uint8Array(buffer).fill(255);
      assert.notStrictEqual(copied.buffer, buffer);
      assert.strictEqual(copied.buffer.resizable ?? copied.buffer.growable, false);
      assert.deepStrictEqual(Array.from(copied), expected);
    });
  }
}

for (const Constructor of [Float16Array, Float32Array, Float64Array]) {
  test(`${Constructor.name} copy preserves raw NaN payloads`, async () => {
    const input = new Constructor(8);
    bytes(input).fill(255);
    const expected = Array.from(bytes(input));
    const copied = await capture(input);

    assert(copied instanceof Constructor);
    assert.deepStrictEqual(Array.from(bytes(copied)), expected);
  });
}

for (const Constructor of [Uint8Array, Buffer]) {
  test(`${Constructor.name} copy ignores shadowed view metadata`, async () => {
    const input = Constructor === Buffer ? Buffer.from([1, 2, 3, 4]) : new Uint8Array([1, 2, 3, 4]);
    Object.defineProperty(input, 'byteLength', { value: 99 });
    for (const key of ['buffer', 'byteOffset', 'length', 'constructor']) {
      Object.defineProperty(input, key, { get: common.mustNotCall(`unexpected ${key} access`) });
    }
    const copied = await capture(input);

    assert.deepStrictEqual(Array.from(bytes(copied)), [1, 2, 3, 4]);
    assert.notStrictEqual(metadata(copied, 'buffer'), metadata(input, 'buffer'));
  });
}

for (const Constructor of [Uint16Array, Float32Array]) {
  test(`${Constructor.name} with Buffer prototype retains its intrinsic type`, async () => {
    const input = new Constructor([1, 2, 3, 4]);
    Object.setPrototypeOf(input, Buffer.prototype);
    assert(Buffer.isBuffer(input));
    const expected = Array.from(bytes(input));
    const copied = await capture(input);

    assert(copied instanceof Constructor);
    assert.strictEqual(Buffer.isBuffer(copied), false);
    assert.deepStrictEqual(Array.from(bytes(copied)), expected);
  });
}

test('cross-realm Uint8Array copy has independent bytes', async () => {
  const input = vm.runInNewContext('new Uint8Array([1, 2, 3, 4])');
  const copied = await capture(input);
  input.fill(255);

  assert(copied instanceof Uint8Array);
  assert.strictEqual(Buffer.isBuffer(copied), false);
  assert.notStrictEqual(copied.buffer, input.buffer);
  assert.deepStrictEqual(Array.from(copied), [1, 2, 3, 4]);
});

test('PassThrough retains private bytes after awaited write', async () => {
  const stream = new PassThrough();
  const writer = Writable.toWeb(stream).getWriter();
  const input = Buffer.from([1, 2, 3, 4]);
  await writer.write(input);
  new Uint8Array(input.buffer).fill(255);
  const result = stream.read();
  const closed = writer.close();
  stream.resume();
  await closed;

  assert.notStrictEqual(result.buffer, input.buffer);
  assert.deepStrictEqual(Array.from(result), [1, 2, 3, 4]);
});
