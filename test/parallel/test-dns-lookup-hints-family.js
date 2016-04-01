'use strict';
require('../common');
const assert = require('assert');

const dns = require('dns');

function noop() {}

dns.setServers([]);

assert.doesNotThrow(function() {
  dns.lookup('www.google.com', {
    family: 4,
    hints: 0
  }, noop);
});

assert.doesNotThrow(function() {
  dns.lookup('www.google.com', {
    family: 6,
    hints: dns.ADDRCONFIG
  }, noop);
});
