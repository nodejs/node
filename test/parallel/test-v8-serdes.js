// Flags: --expose-internals

'use strict';

const { internalBinding } = require('internal/test/binding');
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const v8 = require('v8');
const os = require('os');

const circular = {};
circular.circular = circular;

const wasmModule = new WebAssembly.Module(fixtures.readSync('simple.wasm'));

const objects = [
  { foo: 'bar' },
  { bar: 'baz' },
  new Uint8Array([1, 2, 3, 4]),
  new Uint32Array([1, 2, 3, 4]),
  Buffer.from([1, 2, 3, 4]),
  undefined,
  null,
  42,
  circular,
  wasmModule
];

const hostObject = new (internalBinding('js_stream').JSStream)();

const serializerTypeError =
  /^TypeError: Class constructor Serializer cannot be invoked without 'new'$/;
const deserializerTypeError =
  /^TypeError: Class constructor Deserializer cannot be invoked without 'new'$/;

{
  const ser = new v8.DefaultSerializer();
  ser.writeHeader();
  for (const obj of objects) {
    ser.writeValue(obj);
  }

  const des = new v8.DefaultDeserializer(ser.releaseBuffer());
  des.readHeader();

  for (const obj of objects) {
    assert.deepStrictEqual(des.readValue(), obj);
  }
}

{
  for (const obj of objects) {
    assert.deepStrictEqual(v8.deserialize(v8.serialize(obj)), obj);
  }
}

{
  const ser = new v8.DefaultSerializer();
  ser._getDataCloneError = common.mustCall((message) => {
    assert.strictEqual(message, '[object Object] could not be cloned.');
    return new Error('foobar');
  });

  ser.writeHeader();

  assert.throws(() => {
    ser.writeValue(new Proxy({}, {}));
  }, /foobar/);
}

{
  const ser = new v8.DefaultSerializer();
  ser._writeHostObject = common.mustCall((object) => {
    assert.strictEqual(object, hostObject);
    const buf = Buffer.from('hostObjectTag');

    ser.writeUint32(buf.length);
    ser.writeRawBytes(buf);

    ser.writeUint64(1, 2);
    ser.writeDouble(-0.25);
  });

  ser.writeHeader();
  ser.writeValue({ val: hostObject });

  const des = new v8.DefaultDeserializer(ser.releaseBuffer());
  des._readHostObject = common.mustCall(() => {
    const length = des.readUint32();
    const buf = des.readRawBytes(length);

    assert.strictEqual(buf.toString(), 'hostObjectTag');

    assert.deepStrictEqual(des.readUint64(), [1, 2]);
    assert.strictEqual(des.readDouble(), -0.25);
    return hostObject;
  });

  des.readHeader();

  assert.strictEqual(des.readValue().val, hostObject);
}

// This test ensures that `v8.Serializer.writeRawBytes()` support
// `TypedArray` and `DataView`.
{
  const text = 'hostObjectTag';
  const data = Buffer.from(text);
  const arrayBufferViews = common.getArrayBufferViews(data);

  // `buf` is one of `TypedArray` or `DataView`.
  function testWriteRawBytes(buf) {
    let writeHostObjectCalled = false;
    const ser = new v8.DefaultSerializer();

    ser._writeHostObject = common.mustCall((object) => {
      writeHostObjectCalled = true;
      ser.writeUint32(buf.byteLength);
      ser.writeRawBytes(buf);
    });

    ser.writeHeader();
    ser.writeValue({ val: hostObject });

    const des = new v8.DefaultDeserializer(ser.releaseBuffer());
    des._readHostObject = common.mustCall(() => {
      assert.strictEqual(writeHostObjectCalled, true);
      const length = des.readUint32();
      const buf = des.readRawBytes(length);
      assert.strictEqual(buf.toString(), text);

      return hostObject;
    });

    des.readHeader();

    assert.strictEqual(des.readValue().val, hostObject);
  }

  arrayBufferViews.forEach((buf) => {
    testWriteRawBytes(buf);
  });
}

{
  const ser = new v8.DefaultSerializer();
  ser._writeHostObject = common.mustCall((object) => {
    throw new Error('foobar');
  });

  ser.writeHeader();
  assert.throws(() => {
    ser.writeValue({ val: hostObject });
  }, /foobar/);
}

{
  assert.throws(() => v8.serialize(hostObject),
                /^Error: Unknown host object type: \[object .*\]$/);
}

{
  const buf = Buffer.from('ff0d6f2203666f6f5e007b01', 'hex');

  const des = new v8.DefaultDeserializer(buf);
  des.readHeader();

  const ser = new v8.DefaultSerializer();
  ser.writeHeader();

  ser.writeValue(des.readValue());

  assert.deepStrictEqual(buf, ser.releaseBuffer());
  assert.strictEqual(des.getWireFormatVersion(), 0x0d);
}

{
  // Unaligned Uint16Array read, with padding in the underlying array buffer.
  let buf = Buffer.alloc(32 + 9);
  buf.write('ff0d5c0404addeefbe', 32, 'hex');
  buf = buf.slice(32);

  const expectedResult = os.endianness() === 'LE' ?
    new Uint16Array([0xdead, 0xbeef]) : new Uint16Array([0xadde, 0xefbe]);

  assert.deepStrictEqual(v8.deserialize(buf), expectedResult);
}

{
  assert.throws(v8.Serializer, serializerTypeError);
  assert.throws(v8.Deserializer, deserializerTypeError);
}


// `v8.deserialize()` and `new v8.Deserializer()` should support both
// `TypedArray` and `DataView`.
{
  for (const obj of objects) {
    const buf = v8.serialize(obj);

    for (const arrayBufferView of common.getArrayBufferViews(buf)) {
      assert.deepStrictEqual(v8.deserialize(arrayBufferView), obj);
    }

    for (const arrayBufferView of common.getArrayBufferViews(buf)) {
      const deserializer = new v8.DefaultDeserializer(arrayBufferView);
      deserializer.readHeader();
      const value = deserializer.readValue();
      assert.deepStrictEqual(value, obj);

      const serializer = new v8.DefaultSerializer();
      serializer.writeHeader();
      serializer.writeValue(value);
      assert.deepStrictEqual(buf, serializer.releaseBuffer());
    }
  }
}

{
  const INVALID_SOURCE = 'INVALID_SOURCE_TYPE';
  const serializer = new v8.Serializer();
  serializer.writeHeader();
  assert.throws(
    () => serializer.writeRawBytes(INVALID_SOURCE),
    /^TypeError: source must be a TypedArray or a DataView$/,
  );
  assert.throws(
    () => v8.deserialize(INVALID_SOURCE),
    /^TypeError: buffer must be a TypedArray or a DataView$/,
  );
  assert.throws(
    () => new v8.Deserializer(INVALID_SOURCE),
    /^TypeError: buffer must be a TypedArray or a DataView$/,
  );
}

{
  const deserializedWasmModule = v8.deserialize(v8.serialize(wasmModule));
  const instance = new WebAssembly.Instance(deserializedWasmModule);
  assert.strictEqual(instance.exports.add(10, 20), 30);
}
