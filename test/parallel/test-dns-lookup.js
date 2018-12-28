// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
const dns = require('dns');
const dnsPromises = dns.promises;

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = () => internalBinding('uv').UV_ENOENT;

{
  const err = {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "hostname" argument must be of type string\. Received type number/
  };

  common.expectsError(() => dns.lookup(1, {}), err);
  common.expectsError(() => dnsPromises.lookup(1, {}), err);
}

// This also verifies different expectWarning notations.
common.expectWarning({
  // For 'internal/test/binding' module.
  'internal/test/binding': [
    'These APIs are for internal testing only. Do not use them.'
  ],
  // For dns.promises.
  'ExperimentalWarning': 'The dns.promises API is experimental',
  // For calling `dns.lookup` with falsy `hostname`.
  'DeprecationWarning': {
    DEP0118: 'The provided hostname "false" is not a valid ' +
      'hostname, and is supported in the dns module solely for compatibility.'
  }
});

common.expectsError(() => {
  dns.lookup(false, 'cb');
}, {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
});

common.expectsError(() => {
  dns.lookup(false, 'options', 'cb');
}, {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
});

{
  const err = {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError,
    message: 'The value "100" is invalid for option "hints"'
  };
  const options = {
    hints: 100,
    family: 0,
    all: false
  };

  common.expectsError(() => { dnsPromises.lookup(false, options); }, err);
  common.expectsError(() => {
    dns.lookup(false, options, common.mustNotCall());
  }, err);
}

{
  const err = {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError,
    message: 'The value "20" is invalid for option "family"'
  };
  const options = {
    hints: 0,
    family: 20,
    all: false
  };

  common.expectsError(() => { dnsPromises.lookup(false, options); }, err);
  common.expectsError(() => {
    dns.lookup(false, options, common.mustNotCall());
  }, err);
}

(async function() {
  let res;

  res = await dnsPromises.lookup(false, {
    hints: 0,
    family: 0,
    all: true
  });
  assert.deepStrictEqual(res, []);

  res = await dnsPromises.lookup('127.0.0.1', {
    hints: 0,
    family: 4,
    all: true
  });
  assert.deepStrictEqual(res, [{ address: '127.0.0.1', family: 4 }]);

  res = await dnsPromises.lookup('127.0.0.1', {
    hints: 0,
    family: 4,
    all: false
  });
  assert.deepStrictEqual(res, { address: '127.0.0.1', family: 4 });
})();

dns.lookup(false, {
  hints: 0,
  family: 0,
  all: true
}, common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.deepStrictEqual(result, []);
  assert.strictEqual(addressType, undefined);
}));

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

dns.lookup('127.0.0.1', {
  hints: 0,
  family: 4,
  all: false
}, common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.deepStrictEqual(result, '127.0.0.1');
  assert.strictEqual(addressType, 4);
}));

let tickValue = 0;

dns.lookup('example.com', common.mustCall((error, result, addressType) => {
  assert(error);
  assert.strictEqual(tickValue, 1);
  assert.strictEqual(error.code, 'ENOENT');
  const descriptor = Object.getOwnPropertyDescriptor(error, 'message');
  // The error message should be non-enumerable.
  assert.strictEqual(descriptor.enumerable, false);
}));

// Make sure that the error callback is called
// on next tick.
tickValue = 1;
