/*global SharedArrayBuffer*/
'use strict';

const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

{
  const sab = new SharedArrayBuffer(64);
  const uint8array = new Uint8Array(sab);
  const ID = 42;

  const ser = new v8.Serializer();
  ser._getSharedArrayBufferId = common.mustCall(() => ID);
  ser.writeHeader();

  ser.writeValue(uint8array);

  const des = new v8.Deserializer(ser.releaseBuffer());
  des.readHeader();
  des.transferArrayBuffer(ID, sab);

  const value = des.readValue();
  assert.strictEqual(value.buffer, sab);
  assert.notStrictEqual(value, uint8array);
}
