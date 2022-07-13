'use strict';
// Flags: --expose-internals

require('../common');
const { internalBinding } = require('internal/test/binding');
const assert = require('assert');

// Monkey patch the os binding before requiring any other modules, including
// common, which requires the os module.
internalBinding('os').getHomeDirectory = function(ctx) {
  ctx.syscall = 'foo';
  ctx.code = 'bar';
  ctx.message = 'baz';
};

const os = require('os');

assert.throws(os.homedir, {
  message: /^A system error occurred: foo returned bar \(baz\)$/
});
