'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

function check(e) {
  const clone = v8.deserialize(v8.serialize(e));
  const expected = structuredClone(e);
  assert.strictEqual(Object.getPrototypeOf(clone), Object.getPrototypeOf(e));
  for (const key of ['name', 'message', 'stack', 'code', 'quota', 'requested']) {
    assert.strictEqual(clone[key], expected[key]);
    assert.strictEqual(clone[key], e[key]);
  }
  return clone;
}

check(AbortSignal.abort().reason);
assert.strictEqual(check(new DOMException('msg', 'DataCloneError')).code, 25);
assert.strictEqual(
  check(new QuotaExceededError('msg', { quota: 10, requested: 20 })).quota, 10);

{
  const e = new DOMException('nested', 'NotFoundError');
  const clone = v8.deserialize(v8.serialize({ a: [1, e], b: { e } }));
  assert.strictEqual(clone.a[1].message, 'nested');
  assert.strictEqual(clone.a[1], clone.b.e);
}

{
  // Plain v8.Serializer keeps its previous behavior.
  const ser = new v8.Serializer();
  ser.writeHeader();
  ser.writeValue(new DOMException('msg'));
  const des = new v8.Deserializer(ser.releaseBuffer());
  des.readHeader();
  assert.strictEqual(des.readValue() instanceof DOMException, false);
}
