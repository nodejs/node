// Flags: --expose-internals
'use strict';
const common = require('../common');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
cares.getaddrinfo = () => internalBinding('uv').UV_ENOMEM;

// This test ensures that dns.lookup issue a DeprecationWarning
// when invalid options type is given

const dns = require('dns');
const dnsPromises = dns.promises;

common.expectWarning({
  'internal/test/binding': [
    'These APIs are for internal testing only. Do not use them.',
  ],
  'DeprecationWarning': {
    DEP0153: 'Type coercion of dns.lookup options is deprecated'
  }
});

dnsPromises.lookup('127.0.0.1', { hints: '1024' });
dnsPromises.lookup('127.0.0.1', { family: '6' });
dnsPromises.lookup('127.0.0.1', { all: 'true' });
dnsPromises.lookup('127.0.0.1', { verbatim: 'true' });
dnsPromises.lookup('127.0.0.1', '6');
