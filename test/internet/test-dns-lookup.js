'use strict';

require('../common');
const dnsPromises = require('dns').promises;
const { addresses } = require('../common/internet');
const assert = require('assert');

assert.rejects(
  dnsPromises.lookup(addresses.INVALID_HOST, {
    hints: 0,
    family: 0,
    all: false
  }),
  {
    code: 'ENOTFOUND',
    message: `getaddrinfo ENOTFOUND ${addresses.INVALID_HOST}`
  }
);

assert.rejects(
  dnsPromises.lookup(addresses.INVALID_HOST, {
    hints: 0,
    family: 0,
    all: true
  }),
  {
    code: 'ENOTFOUND',
    message: `getaddrinfo ENOTFOUND ${addresses.INVALID_HOST}`
  }
);
