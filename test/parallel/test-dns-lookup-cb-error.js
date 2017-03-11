'use strict';
require('../common');
const assert = require('assert');
const cares = process.binding('cares_wrap');

const dns = require('dns');

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = function() {
  return process.binding('uv').UV_ENOENT;
};

assert.doesNotThrow(function() {
  let tickValue = 0;

  dns.lookup('example.com', function(error, result, addressType) {
    assert.equal(tickValue, 1);
    assert.equal(error.code, 'ENOENT');
  });

  // Make sure that the error callback is called
  // on next tick.
  tickValue = 1;
});
