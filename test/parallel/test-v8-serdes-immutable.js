// Flags: --js-immutable-arraybuffer
'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

function immutable(bytes) {
  const ab = new ArrayBuffer(bytes.length);
  new Uint8Array(ab).set(bytes);
  return ab.transferToImmutable();
}

function bytesOf(view) {
  return [...new Uint8Array(view.buffer, view.byteOffset, view.byteLength)];
}

const source = immutable([9, 9, 65, 66, 67, 68, 9, 9]);

// A whole immutable view keeps its type, its bytes and its immutability.
{
  const cases = [
    [new Uint8Array(source), 1],
    [Buffer.from(source), 1],
    [new Uint16Array(source), 2],
    [new DataView(source), 1],
    [new Float64Array(source), 8],
  ];
  for (const [view, bytesPerElement] of cases) {
    const result = v8.deserialize(v8.serialize(view));
    assert.strictEqual(result.constructor, view.constructor);
    assert.strictEqual(result.buffer.immutable, true);
    assert.strictEqual(result.byteOffset, 0);
    assert.strictEqual(result.byteLength, 8);
    assert.strictEqual(result.length ?? 8, 8 / bytesPerElement);
    assert.deepStrictEqual(bytesOf(result), [9, 9, 65, 66, 67, 68, 9, 9]);
  }
}

// A partial view is refused, an explicit sliceToImmutable() copy serializes.
{
  const secret = immutable([...Buffer.from('PREFIX|PUBLIC|SUFFIX')]);
  assert.throws(() => v8.serialize(new Uint8Array(secret, 7, 6)), {
    name: 'Error',
    message: /Serialize a view over buffer\.sliceToImmutable\(start, end\) instead/,
  });
  const serialized = v8.serialize(new Uint8Array(secret.sliceToImmutable(7, 13)));
  assert.strictEqual(Buffer.from(serialized).includes('PUBLIC'), true);
  assert.strictEqual(Buffer.from(serialized).includes('PREFIX'), false);
  assert.strictEqual(Buffer.from(serialized).includes('SUFFIX'), false);
  const result = v8.deserialize(serialized);
  assert.strictEqual(result.buffer.immutable, true);
  assert.deepStrictEqual(bytesOf(result), [...Buffer.from('PUBLIC')]);
}

// A masked byteLength does not make a partial view look whole.
{
  const buffer = immutable([...Buffer.from('PUBLIC|SECRET')]);
  const view = new Uint8Array(buffer, 0, 6);
  Object.defineProperty(buffer, 'byteLength', { value: 6 });
  assert.throws(() => v8.serialize(view), {
    message: /view over part of an immutable ArrayBuffer/,
  });
}

// Views sharing a whole buffer still share it after the round trip.
{
  const result = v8.deserialize(v8.serialize({
    first: new Uint8Array(source),
    second: new DataView(source),
  }));
  assert.strictEqual(result.first.buffer, result.second.buffer);
  assert.strictEqual(result.first.buffer.immutable, true);
}

// Mutable and immutable views mix, with repeated references and a cycle.
{
  const mutable = new Uint8Array([1, 2, 3]);
  const value = { mutable, immutable: new Uint8Array(source), again: mutable };
  value.self = value;
  const result = v8.deserialize(v8.serialize(value));
  assert.strictEqual(result.mutable.buffer.immutable, false);
  assert.strictEqual(result.immutable.buffer.immutable, true);
  assert.strictEqual(result.mutable, result.again);
  assert.strictEqual(result.self, result);
}

// Only the mutable view keeps aliasing the transport bytes.
{
  const serialized = v8.serialize({
    mutable: new Uint8Array([5, 5]),
    immutable: new Uint8Array(immutable([65, 66])),
  });
  const result = v8.deserialize(serialized);
  serialized.fill(0);
  assert.deepStrictEqual(bytesOf(result.immutable), [65, 66]);
  assert.deepStrictEqual(bytesOf(result.mutable), [0, 0]);
}

// A mutable view read from an immutable transport comes back mutable.
{
  const serialized = v8.serialize(new Uint8Array([7, 8]));
  const transport = new Uint8Array(immutable([...serialized]));
  const result = v8.deserialize(transport);
  assert.strictEqual(result.buffer.immutable, false);
  assert.deepStrictEqual(bytesOf(result), [7, 8]);
}

// SharedArrayBuffer views and transport keep working.
{
  const shared = new SharedArrayBuffer(4);
  new Uint8Array(shared).set([1, 2, 3, 4]);
  for (const view of [new Uint8Array(shared), new DataView(shared), Buffer.from(shared)]) {
    const result = v8.deserialize(v8.serialize(view));
    assert.strictEqual(result.constructor, view.constructor);
    assert.deepStrictEqual(bytesOf(result), [1, 2, 3, 4]);
  }
  const serialized = v8.serialize(new Uint8Array([6, 7]));
  const transport = new Uint8Array(new SharedArrayBuffer(serialized.byteLength));
  transport.set(serialized);
  assert.deepStrictEqual(bytesOf(v8.deserialize(transport)), [6, 7]);
}

// Malformed records are rejected with ERR_INVALID_STATE.
{
  class BadSerializer extends v8.Serializer {
    constructor(write) {
      super();
      this._setTreatArrayBufferViewsAsHostObjects(true);
      this._writeHostObject = write;
    }
  }
  const serializeWith = (write) => {
    const serializer = new BadSerializer(write);
    serializer.writeHeader();
    serializer.writeValue(new Uint8Array(1));
    return serializer.releaseBuffer();
  };
  const unknownIndex = serializeWith(function() {
    this.writeUint32(14);
    this.writeUint32(0);
  });
  assert.throws(() => v8.deserialize(unknownIndex), {
    code: 'ERR_INVALID_STATE',
    message: /unknown ArrayBufferView type index 14/,
  });
  const notABuffer = serializeWith(function() {
    this.writeUint32(1 | 0x80000000);
    this.writeValue(4);
  });
  assert.throws(() => v8.deserialize(notABuffer), {
    code: 'ERR_INVALID_STATE',
    message: /expected an ArrayBuffer value/,
  });
  const wrongLength = serializeWith(function() {
    this.writeUint32(4 | 0x80000000);
    this.writeValue(immutable([1, 2, 3]));
  });
  assert.throws(() => v8.deserialize(wrongLength), {
    code: 'ERR_INVALID_STATE',
    message: /3 bytes do not fit Uint16Array/,
  });
}

// Symbol.species is not consulted, aligned or not.
{
  const serialized = v8.serialize(new Uint16Array([257, 514]));
  Object.defineProperty(Uint8Array, Symbol.species, {
    configurable: true,
    get: common.mustNotCall('Symbol.species must not be consulted'),
  });
  for (const shift of [0, 1]) {
    const transport = new Uint8Array(serialized.byteLength + shift);
    transport.set(serialized, shift);
    const result = v8.deserialize(
      new Uint8Array(transport.buffer, shift, serialized.byteLength));
    assert.deepStrictEqual([...result], [257, 514]);
  }
  delete Uint8Array[Symbol.species];
}

// Without the flag, an immutable view read from a record serializes again.
{
  const { spawnSync } = require('child_process');
  const record = v8.serialize(new Uint8Array(source)).toString('hex');
  const child = spawnSync(process.execPath, ['-e', `
    const v8 = require('v8');
    const whole = v8.deserialize(Buffer.from('${record}', 'hex'));
    whole[0] = 0;
    if (whole[0] === 0) throw new Error('expected an immutable buffer');
    const again = v8.deserialize(v8.serialize(whole));
    again[0] = 0;
    if (again[0] === 0) throw new Error('expected an immutable buffer again');
    console.log(String.fromCharCode(...again.subarray(2, 6)));
  `], { encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
  assert.strictEqual(child.stdout.trim(), 'ABCD');
}
