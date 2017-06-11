'use strict';
const common = require('../common');
const assert = require('assert');
const cares = process.binding('cares_wrap');
const dns = require('dns');

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = () => process.binding('uv').UV_ENOENT;

assert.throws(() => {
  dns.lookup(1, {});
}, /^TypeError: Invalid arguments: hostname must be a string or falsey$/);

assert.throws(() => {
  dns.lookup(false, 'cb');
}, /^TypeError: Invalid arguments: callback must be passed$/);

assert.throws(() => {
  dns.lookup(false, 'options', 'cb');
}, /^TypeError: Invalid arguments: callback must be passed$/);

assert.throws(() => {
  dns.lookup(false, {
    hints: 100,
    family: 0,
    all: false
  }, common.mustNotCall());
}, /^TypeError: Invalid argument: hints must use valid flags$/);

assert.throws(() => {
  dns.lookup(false, {
    hints: 0,
    family: 20,
    all: false
  }, common.mustNotCall());
}, /^TypeError: Invalid argument: family must be 4 or 6$/);

assert.doesNotThrow(() => {
  dns.lookup(false, {
    hints: 0,
    family: 0,
    all: true
  }, common.mustCall((error, result, addressType) => {
    assert.ifError(error);
    assert.deepStrictEqual(result, []);
    assert.strictEqual(addressType, undefined);
  }));
});

assert.doesNotThrow(() => {
  dns.lookup('127.0.0.1', {
    hints: 0,
    family: 4,
    all: true
  }, common.mustCall((error, result, addressType) => {
    assert.ifError(error);
    assert.deepStrictEqual(result, [{
      address: '127.0.0.1',
      family: 4
    }]);
    assert.strictEqual(addressType, undefined);
  }));
});

assert.doesNotThrow(() => {
  dns.lookup('127.0.0.1', {
    hints: 0,
    family: 4,
    all: false
  }, common.mustCall((error, result, addressType) => {
    assert.ifError(error);
    assert.deepStrictEqual(result, '127.0.0.1');
    assert.strictEqual(addressType, 4);
  }));
});

assert.doesNotThrow(() => {
  let tickValue = 0;

  dns.lookup('example.com', common.mustCall((error, result, addressType) => {
    assert(error);
    assert.strictEqual(tickValue, 1);
    assert.strictEqual(error.code, 'ENOENT');
  }));

  // Make sure that the error callback is called
  // on next tick.
  tickValue = 1;
});
