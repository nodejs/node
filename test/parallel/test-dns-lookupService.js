// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
const { UV_ENOENT } = internalBinding('uv');

// Stub `getnameinfo` to *always* error.
cares.getnameinfo = () => UV_ENOENT;

// Because dns promises is attached lazily,
// and turn accesses getnameinfo on init
// but this lazy access is triggered by ES named
// instead of lazily itself, we must require
// dns after hooking cares
const dns = require('dns');

assert.throws(
  () => dns.lookupService('127.0.0.1', 80, common.mustNotCall()),
  {
    code: 'ENOENT',
    message: 'getnameinfo ENOENT 127.0.0.1',
    syscall: 'getnameinfo'
  }
);

assert.rejects(
  dns.promises.lookupService('127.0.0.1', 80),
  {
    code: 'ENOENT',
    message: 'getnameinfo ENOENT 127.0.0.1',
    syscall: 'getnameinfo'
  }
);
