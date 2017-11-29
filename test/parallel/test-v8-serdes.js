'use strict';

const common = require('../common');
const assert = require('assert');
const v8 = require('v8');
const os = require('os');

const circular = {};
circular.circular = circular;

const objects = [
  { foo: 'bar' },
  { bar: 'baz' },
  new Uint8Array([1, 2, 3, 4]),
  new Uint32Array([1, 2, 3, 4]),
  Buffer.from([1, 2, 3, 4]),
  undefined,
  null,
  42,
  circular
];

const hostObject = new (process.binding('js_stream').JSStream)();

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
