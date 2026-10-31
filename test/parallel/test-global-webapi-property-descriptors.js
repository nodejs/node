'use strict';

// Verify that global Web API constructors (FormData, Headers, Request,
// Response) have property descriptors that match the Web IDL specification.
// Specifically, they should be data properties that are writable,
// non-enumerable, and configurable.
// See: https://webidl.spec.whatwg.org/#es-interfaces
// Fixes: https://github.com/nodejs/node/issues/45099

const common = require('../common');
const assert = require('assert');

// Web API interfaces exposed on globalThis per Web IDL spec
const webAPIInterfaces = [
  'Headers',
  'Request',
  'Response',
  'FormData',
];

for (const name of webAPIInterfaces) {
  const desc = Object.getOwnPropertyDescriptor(globalThis, name);

  assert.notStrictEqual(desc, undefined,
    `globalThis should have own property '${name}'`);

  // Must be a data descriptor (has 'value'), not an accessor descriptor
  assert.ok('value' in desc,
    `globalThis.${name} descriptor should be a data property`);

  // The value should be the constructor
  assert.strictEqual(desc.value, globalThis[name],
    `globalThis.${name} descriptor value should match globalThis.${name}`);

  // Per Web IDL: writable = true
  assert.strictEqual(desc.writable, true,
    `globalThis.${name} should be writable`);

  // Per Web IDL: enumerable = false
  assert.strictEqual(desc.enumerable, false,
    `globalThis.${name} should not be enumerable`);

  // Per Web IDL: configurable = true
  assert.strictEqual(desc.configurable, true,
    `globalThis.${name} should be configurable`);

  // Verify the value is actually a function (constructor)
  assert.strictEqual(typeof desc.value, 'function',
    `globalThis.${name} should be a function`);
}

// Also verify that these properties can be overwritten (writable)
for (const name of webAPIInterfaces) {
  const original = globalThis[name];
  const replacement = function replacement() {};
  globalThis[name] = replacement;
  assert.strictEqual(globalThis[name], replacement,
    `globalThis.${name} should be overwritable`);
  // Restore original
  globalThis[name] = original;
}

// Also verify that these properties can be deleted (configurable)
for (const name of webAPIInterfaces) {
  const original = globalThis[name];
  assert.ok(delete globalThis[name],
    `globalThis.${name} should be deletable`);
  assert.strictEqual(globalThis[name], undefined,
    `globalThis.${name} should be undefined after deletion`);
  // Restore
  Object.defineProperty(globalThis, name, {
    __proto__: null,
    writable: true,
    enumerable: false,
    configurable: true,
    value: original,
  });
}

// Verify these are not enumerable (should not appear in for-in)
const enumerableKeys = [];
for (const key in globalThis) {
  if (webAPIInterfaces.includes(key)) {
    enumerableKeys.push(key);
  }
}
assert.deepStrictEqual(enumerableKeys, [],
  'Web API interfaces should not be enumerable on globalThis');
