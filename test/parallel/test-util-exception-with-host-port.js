'use strict';

require('../common');
const assert = require('node:assert');
const os = require('node:os');
const util = require('node:util');

const ENOENT = -os.constants.errno.ENOENT;
const EACCES = -os.constants.errno.EACCES;

// Address and port are included in the message
{
  const e = util._exceptionWithHostPort(ENOENT, 'connect', '127.0.0.1', 8080);
  assert.ok(e instanceof Error);
  assert.strictEqual(e.errno, ENOENT);
  assert.strictEqual(e.code, util.getSystemErrorName(ENOENT));
  assert.strictEqual(e.syscall, 'connect');
  assert.strictEqual(e.address, '127.0.0.1');
  assert.strictEqual(e.port, 8080);
  assert.ok(e.message.includes('127.0.0.1:8080'), `expected 127.0.0.1:8080 in "${e.message}"`);
}

// Port=0 omits host:port detail; port property is not set
{
  const e = util._exceptionWithHostPort(ENOENT, 'connect', '127.0.0.1', 0);
  assert.strictEqual(e.address, '127.0.0.1');
  assert.strictEqual(e.port, undefined);
  assert.ok(e.message.includes('127.0.0.1'), `expected address in "${e.message}"`);
  assert.ok(!e.message.includes(':0'), `unexpected :0 in "${e.message}"`);
}

// Additional is appended as "- Local (...)"
{
  const e = util._exceptionWithHostPort(ENOENT, 'connect', '127.0.0.1', 80, '0.0.0.0:12345');
  assert.ok(e.message.includes('- Local (0.0.0.0:12345)'), `expected local info in "${e.message}"`);
}

// No address or port
{
  const e = util._exceptionWithHostPort(ENOENT, 'connect', null, 0);
  assert.strictEqual(e.address, null);
  assert.strictEqual(e.port, undefined);
  assert.ok(!e.message.includes('null'), `unexpected null in "${e.message}"`);
}

// Different syscall and error code
{
  const e = util._exceptionWithHostPort(EACCES, 'bind', '0.0.0.0', 443);
  assert.strictEqual(e.code, util.getSystemErrorName(EACCES));
  assert.strictEqual(e.syscall, 'bind');
}

// Stack trace should not include _exceptionWithHostPort itself
{
  const e = util._exceptionWithHostPort(ENOENT, 'connect', '127.0.0.1', 80);
  assert.ok(!e.stack.includes('_exceptionWithHostPort'), 'stack must not mention the wrapper');
}
