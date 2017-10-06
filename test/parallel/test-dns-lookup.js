'use strict';
const common = require('../common');
const assert = require('assert');
const cares = process.binding('cares_wrap');
const dns = require('dns');

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = () => process.binding('uv').UV_ENOENT;

assert.throws(() => {
  dns.lookup(1, {});
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "hostname" argument must be one of type string or falsey/
}));

assert.throws(() => {
  dns.lookup(false, 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
}));

assert.throws(() => {
  dns.lookup(false, 'options', 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
}));

assert.throws(() => {
  dns.lookup(false, {
    hints: 100,
    family: 0,
    all: false
  }, common.mustNotCall());
}, common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: TypeError,
  message: 'The value "100" is invalid for option "hints"'
}));

assert.throws(() => {
  dns.lookup(false, {
    hints: 0,
    family: 20,
    all: false
  }, common.mustNotCall());
}, common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: TypeError,
  message: 'The value "20" is invalid for option "family"'
}));

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
