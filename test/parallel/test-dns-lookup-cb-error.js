'use strict';
// Flags: --expose_internals
var common = require('../common');
var assert = require('assert');
var cares = require('binding/cares_wrap');

var dns = require('dns');

// Stub `getaddrinfo` to *always* error.
cares.getaddrinfo = function() {
  return require('binding/uv').UV_ENOENT;
};

assert.doesNotThrow(function() {
  var tickValue = 0;

  dns.lookup('example.com', function(error, result, addressType) {
    assert.equal(tickValue, 1);
    assert.equal(error.code, 'ENOENT');
  });

  // Make sure that the error callback is called
  // on next tick.
  tickValue = 1;
});
