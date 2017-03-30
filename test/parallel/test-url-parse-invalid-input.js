'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

// https://github.com/joyent/node/issues/568
[
  undefined,
  null,
  true,
  false,
  0.0,
  0,
  [],
  {}
].forEach(function(val) {
  assert.throws(function() { url.parse(val); }, TypeError);
});

assert.throws(function() { url.parse('http://%E0%A4%A@fail'); }, /^URIError: URI malformed$/);
