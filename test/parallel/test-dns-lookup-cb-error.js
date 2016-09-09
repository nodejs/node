'use strict';
const common = require('../common');
const assert = require('assert');
const cares = process.binding('cares_wrap');

const dns = require('dns');

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = function() {
  return process.binding('uv').UV_ENOENT;
};

assert.doesNotThrow(() => {
  var tickValue = 0;

  dns.lookup('example.com', common.mustCall((error, result, addressType) => {
    assert(error);
    assert.strictEqual(tickValue, 1);
    assert.strictEqual(error.code, 'ENOENT');
  }));

  // Make sure that the error callback is called
  // on next tick.
  tickValue = 1;
});
