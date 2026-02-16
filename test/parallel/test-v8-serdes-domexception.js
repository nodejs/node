'use strict';

const assert = require('node:assert');
const { serialize, deserialize } = require('node:v8');

// Simple case, should preserve message/name/stack like structuredClone
{
  const e = AbortSignal.abort().reason;

  const cloned = deserialize(serialize(e));

  assert(cloned instanceof DOMException);
  assert.strictEqual(cloned.name, e.name);
  assert.strictEqual(cloned.message, e.message);

  assert.strictEqual(typeof cloned.stack, 'string');
  assert(cloned.stack.includes(e.name));
}

// Nested case
{
  const e = AbortSignal.abort().reason;
  const obj = { e };

  const clonedObj = deserialize(serialize(obj));
  assert(clonedObj.e instanceof DOMException);
  assert.strictEqual(clonedObj.e.name, e.name);
  assert.strictEqual(clonedObj.e.message, e.message);
}
