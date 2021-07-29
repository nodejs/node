'use strict';
require('../common');
const assert = require('assert');
const { cpus } = require('os');
const { platform } = require('process');

assert.ok(navigator.deviceMemory >= 0.25 && navigator.deviceMemory <= 8);

assert.strictEqual(navigator.platform, {
  aix: 'AIX',
  android: 'Android',
  darwin: 'Darwin',
  freebsd: 'FreeBSD',
  linux: 'Linux',
  openbsd: 'OpenBSD',
  sunos: 'SunOS',
  win32: 'Win32',
}[platform]);

assert.ok(navigator.hardwareConcurrency >= 1);
assert.strictEqual(navigator.hardwareConcurrency, cpus().length);
