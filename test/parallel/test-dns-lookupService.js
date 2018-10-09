// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
const { UV_ENOENT } = internalBinding('uv');
const dns = require('dns');

// Stub `getnameinfo` to *always* error.
cares.getnameinfo = () => UV_ENOENT;

assert.throws(
  () => dns.lookupService('127.0.0.1', 80, common.mustNotCall()),
  {
    code: 'ENOENT',
    message: 'getnameinfo ENOENT 127.0.0.1',
    syscall: 'getnameinfo'
  }
);
