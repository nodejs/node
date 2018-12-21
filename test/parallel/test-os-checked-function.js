'use strict';

// Monkey patch the os binding before requiring any other modules, including
// common, which requires the os module.
if (!process.execArgv.includes('--expose-internals')) {
  require('../common').relaunchWithFlags(['--expose-internals']);
}
const { internalBinding } = require('internal/test/binding');
internalBinding('os').getHomeDirectory = function(ctx) {
  ctx.syscall = 'foo';
  ctx.code = 'bar';
  ctx.message = 'baz';
};

// Now that we have the os binding monkey-patched, proceed normally.

const common = require('../common');
const os = require('os');

common.expectsError(os.homedir, {
  message: /^A system error occurred: foo returned bar \(baz\)$/
});
