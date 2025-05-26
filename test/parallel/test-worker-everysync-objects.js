// Flags: --expose-internals
'use strict';

// eslint-disable-next-line no-unused-vars
const common = require('../common');
const assert = require('assert');
const { read, write } = require('internal/worker/everysync/objects');

// Test basic serialization/deserialization
{
  const obj = { foo: 'bar' };
  const buffer = new SharedArrayBuffer(1024);
  write(buffer, obj);
  const obj2 = read(buffer);
  assert.deepStrictEqual(obj, obj2);
}

// Test serialization/deserialization with offset
{
  const obj = { foo: 'bar' };
  const buffer = new SharedArrayBuffer(1024);
  write(buffer, obj, 4);
  const obj2 = read(buffer, 4);
  assert.deepStrictEqual(obj, obj2);
}

// Test insufficient buffer size
{
  const obj = { foo: 'bar' };
  const buffer = new SharedArrayBuffer(10);
  assert.throws(() => write(buffer, obj), {
    name: 'TypeError',
  });
}

// Test with growable buffer
{
  const obj = { foo: 'bar' };
  const buffer = new SharedArrayBuffer(2, {
    maxByteLength: 1024,
  });
  write(buffer, obj);
  const obj2 = read(buffer);
  assert.deepStrictEqual(obj, obj2);
}

// Test with growable buffer and offset
{
  const obj = { foo: 'bar' };
  const buffer = new SharedArrayBuffer(2, {
    maxByteLength: 1024,
  });
  write(buffer, obj, 4);
  const obj2 = read(buffer, 4);
  assert.deepStrictEqual(obj, obj2);
}

// Test complex objects
{
  const obj = {
    string: 'hello',
    number: 42,
    boolean: true,
    null: null,
    array: [1, 2, 3],
    nested: {
      a: 1,
      b: [true, false]
    }
  };

  const buffer = new SharedArrayBuffer(1024);
  write(buffer, obj);
  const obj2 = read(buffer);
  assert.deepStrictEqual(obj, obj2);
}
