'use strict';
// Flags: --expose_internals

const { internalBinding } = require('internal/test/binding');

// Monkey patch the os binding before requiring any other modules, including
// common, which requires the os module.
internalBinding('os').getHomeDirectory = function(ctx) {
  ctx.syscall = 'foo';
  ctx.code = 'bar';
  ctx.message = 'baz';
};

const common = require('../common');
const os = require('os');

common.expectsError(os.homedir, {
  message: /^A system error occurred: foo returned bar \(baz\)$/
});
