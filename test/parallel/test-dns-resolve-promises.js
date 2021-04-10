// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { DNSWrap } = internalBinding('dns');
const { UV_EPERM } = internalBinding('uv');
const dnsPromises = require('dns').promises;

// Stub cares to force an error so we can test DNS error code path.
DNSWrap.prototype.queryA = () => UV_EPERM;

assert.rejects(
  dnsPromises.resolve('example.org'),
  {
    code: 'EPERM',
    syscall: 'queryA',
    hostname: 'example.org'
  }
);
