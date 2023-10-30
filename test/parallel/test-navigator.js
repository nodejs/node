// Flags: --expose-internals

'use strict';

require('../common');
const assert = require('assert');
const {
  getApproximatedDeviceMemory,
} = require('internal/navigator');

const is = {
  number: (value, key) => {
    assert(!Number.isNaN(value), `${key} should not be NaN`);
    assert.strictEqual(typeof value, 'number');
  },
};

assert.strictEqual(typeof navigator.deviceMemory, 'number');
assert.ok(navigator.deviceMemory >= 0);

assert.strictEqual(getApproximatedDeviceMemory(0), 0);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 27), 0.125);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 28), 0.25);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 29), 0.5);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 30), 1);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 31), 2);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 32), 4);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 33), 8);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 34), 16);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 35), 32);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 36), 64);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 37), 128);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 38), 256);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 39), 512);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 40), 1024);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 41), 2048);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 42), 4096);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 43), 8192);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 44), 16384);
assert.strictEqual(getApproximatedDeviceMemory(2 ** 45), 32768);

// https://github.com/w3c/device-memory#the-header
assert.strictEqual(getApproximatedDeviceMemory(768 * 1024 * 1024), 0.5);
assert.strictEqual(getApproximatedDeviceMemory(1793 * 1024 * 1024), 2);

is.number(+navigator.hardwareConcurrency, 'hardwareConcurrency');
is.number(navigator.hardwareConcurrency, 'hardwareConcurrency');
assert.ok(navigator.hardwareConcurrency > 0);
assert.strictEqual(typeof navigator.userAgent, 'string');
assert.match(navigator.userAgent, /^Node\.js\/\d+$/);
