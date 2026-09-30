// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');
const { defineLazyProperties } = require('internal/util');

for (const enumerable of [true, false]) {
  const source = Object.setPrototypeOf({ value: 'path' }, null);
  const target = {};
  if (enumerable) {
    defineLazyProperties(target, source, ['value']);
  } else {
    defineLazyProperties(target, source, ['value'], false);
  }
  assert.deepStrictEqual(Object.keys(target), enumerable ? ['value'] : []);
  assert.deepStrictEqual(Object.getOwnPropertyNames(target), ['value']);

  // Retrieving the descriptor resolves a lazy data property, not an accessor.
  const value = require('path');
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(target, 'value'), {
    value, writable: true, enumerable, configurable: true,
  });
  assert.strictEqual(target.value, value);
  assert.strictEqual(target.value, value);
}

{
  // Assignment must not try to load the module.
  const target = {};
  defineLazyProperties(target, { value: 'nonexistent-builtin' }, ['value']);
  target.value = 42;
  assert.strictEqual(target.value, 42);
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(target, 'value'), {
    value: 42, writable: true, enumerable: true, configurable: true,
  });
}

{
  const target = {};
  defineLazyProperties(target, { value: 'path' }, ['value']);
  Object.freeze(target);
  const value = require('path');
  assert.strictEqual(target.value, value);
  assert.strictEqual(target.value, value);
  assert.deepStrictEqual(Object.getOwnPropertyDescriptor(target, 'value'), {
    value, writable: false, enumerable: true, configurable: false,
  });
}

{
  const target = {};
  defineLazyProperties(target, { value: 'nonexistent-builtin' }, ['value']);
  // Failed loads must propagate the error and allow another attempt.
  for (let i = 0; i < 2; i++) {
    assert.throws(() => target.value, {
      name: 'TypeError',
      message: "Missing internal module 'nonexistent-builtin'",
    });
  }
  target.value = 42;
  assert.strictEqual(target.value, 42);
}

{
  const target = {};
  defineLazyProperties(target, { value: 'path' }, ['value']);
  const child = { __proto__: target };
  const value = vm.runInNewContext('child.value', { child });
  assert.strictEqual(value, require('path'));
  assert.strictEqual(Object.hasOwn(child, 'value'), false);
  assert.strictEqual(target.value, value);
}

{
  // Multiple source maps and named exports can share the same target.
  const target = {};
  defineLazyProperties(target, { path: 'path', buffer: 'buffer' }, ['path', 'buffer']);
  defineLazyProperties(target, { util: 'util' }, ['util']);
  defineLazyProperties(target, 'path', ['join']);
  assert.strictEqual(target.path, require('path'));
  assert.strictEqual(target.buffer, require('buffer'));
  assert.strictEqual(target.util, require('util'));
  assert.strictEqual(vm.runInNewContext('target.join', { target }), require('path').join);
}

{
  const error = new Error('source getter');
  const source = {
    get value() {
      throw error;
    },
  };
  assert.throws(() => defineLazyProperties({}, source, ['value']), (err) => err === error);
}
