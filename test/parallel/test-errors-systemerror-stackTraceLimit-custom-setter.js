// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { E, SystemError, codes } = require('internal/errors');

let stackTraceLimit;
Reflect.defineProperty(Error, 'stackTraceLimit', {
  get() { return stackTraceLimit; },
  set(value) { stackTraceLimit = value; },
});

E('ERR_TEST', 'custom message', SystemError);
const { ERR_TEST } = codes;

const ctx = {
  code: 'ETEST',
  message: 'code message',
  syscall: 'syscall_test',
  path: '/str',
  dest: '/str2'
};
assert.throws(
  () => { throw new ERR_TEST(ctx); },
  {
    code: 'ERR_TEST',
    name: 'SystemError',
    info: ctx,
  }
);
