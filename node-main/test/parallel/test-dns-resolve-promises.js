// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
const { UV_EPERM } = internalBinding('uv');
const dnsPromises = require('dns').promises;

// Stub cares to force an error so we can test DNS error code path.
cares.ChannelWrap.prototype.queryA = () => UV_EPERM;

assert.rejects(
  dnsPromises.resolve('example.org'),
  {
    code: 'EPERM',
    syscall: 'queryA',
    hostname: 'example.org'
  }
).then(common.mustCall());
