// Flags: --expose-internals

'use strict';

const common = require('../common');
const { internalBinding } = require('internal/test/binding');
const assert = require('assert');
const v8 = require('v8');
const os = require('os');

const circular = {};
circular.circular = circular;

const objects = [
  { foo: 'bar' },
  { bar: 'baz' },
  new Int8Array([1, 2, 3, 4]),
  new Uint8Array([1, 2, 3, 4]),
  new Int16Array([1, 2, 3, 4]),
  new Uint16Array([1, 2, 3, 4]),
  new Int32Array([1, 2, 3, 4]),
  new Uint32Array([1, 2, 3, 4]),
  new Float32Array([1, 2, 3, 4]),
  new Float64Array([1, 2, 3, 4]),
  new DataView(new ArrayBuffer(42)),
  Buffer.from([1, 2, 3, 4]),
  new BigInt64Array([42n]),
  new BigUint64Array([42n]),
  new Float16Array([1, 2, 3, 4]),
  undefined,
  null,
  42,
  circular,
];

const hostObject = new (internalBinding('js_stream').JSStream)();

{
  const set = new v8.DefaultSerializer();
  set.writeHeader();
  for (const obj of objects) {
    set.writeValue(obj);
  }

  const des = new v8.DefaultDeserializer(set.releaseBuffer());
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
  const set = new v8.DefaultSerializer();
  set._getDataCloneError = common.mustCall((message) => {
    assert.strictEqual(message, '#<Object> could not be cloned.');
    return new Error('foobar');
  });

  set.writeHeader();

  assert.throws(() => {
    set.writeValue(new Proxy({}, {}));
  }, /foobar/);
}

{
  const set = new v8.DefaultSerializer();
  set._writeHostObject = common.mustCall((object) => {
    assert.strictEqual(object, hostObject);
    const buf = Buffer.from('hostObjectTag');

    set.writeUint32(buf.length);
    set.writeRawBytes(buf);

    set.writeUint64(1, 2);
    set.writeDouble(-0.25);
  });

  set.writeHeader();
  set.writeValue({ val: hostObject });

  const des = new v8.DefaultDeserializer(set.releaseBuffer());
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

  // `buf` is one of `TypedArray` or `DataView`.
  function testWriteRawBytes(buf) {
    let writeHostObjectCalled = false;
    const set = new v8.DefaultSerializer();

    set._writeHostObject = common.mustCall((object) => {
      writeHostObjectCalled = true;
      set.writeUint32(buf.byteLength);
      set.writeRawBytes(buf);
    });

    set.writeHeader();
    set.writeValue({ val: hostObject });

    const des = new v8.DefaultDeserializer(set.releaseBuffer());
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

  for (const buf of common.getArrayBufferViews(data)) {
    testWriteRawBytes(buf);
  }
}

{
  const set = new v8.DefaultSerializer();
  set._writeHostObject = common.mustCall((object) => {
    throw new Error('foobar');
  });

  set.writeHeader();
  assert.throws(() => {
    set.writeValue({ val: hostObject });
  }, /foobar/);
}

{
  assert.throws(() => v8.serialize(hostObject), {
    constructor: Error,
    message: 'Unserializable host object: JSStream {}'
  });
}

{
  // Test that an old serialized value can still be deserialized.
  const buf = Buffer.from('ff0d6f2203666f6f5e007b01', 'hex');

  const des = new v8.DefaultDeserializer(buf);
  des.readHeader();
  assert.strictEqual(des.getWireFormatVersion(), 0x0d);

  const value = des.readValue();
  assert.strictEqual(value, value.foo);
}

{
  const message = `New serialization format.

    This test is expected to fail when V8 changes its serialization format.
    When that happens, the "desStr" variable must be updated to the new value
    and the change should be mentioned in the release notes, as it is semver-major.

    Consider opening an issue as a heads up at https://github.com/nodejs/node/issues/new
  `;

  const desStr = 'ff0f6f2203666f6f5e007b01';

  const desBuf = Buffer.from(desStr, 'hex');
  const des = new v8.DefaultDeserializer(desBuf);
  des.readHeader();
  const value = des.readValue();

  const set = new v8.DefaultSerializer();
  set.writeHeader();
  set.writeValue(value);

  const serBuf = set.releaseBuffer();
  const serStr = serBuf.toString('hex');
  assert.deepStrictEqual(serStr, desStr, message);
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
  assert.throws(() => v8.Serializer(), {
    constructor: TypeError,
    message: "Class constructor Serializer cannot be invoked without 'new'",
    code: 'ERR_CONSTRUCT_CALL_REQUIRED'
  });
  assert.throws(() => v8.Deserializer(), {
    constructor: TypeError,
    message: "Class constructor Deserializer cannot be invoked without 'new'",
    code: 'ERR_CONSTRUCT_CALL_REQUIRED'
  });
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
  // Regression test for https://github.com/nodejs/node/issues/37978
  assert.throws(() => {
    new v8.Deserializer(new v8.Serializer().releaseBuffer()).readDouble();
  }, /ReadDouble\(\) failed/);
}
