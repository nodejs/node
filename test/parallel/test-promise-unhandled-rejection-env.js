// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

const env = 'FOO';

if (!common.isWindows) {
  // Verify that a faulty environment variable throws on bootstrapping.
  // Therefore we do not need any special handling for the child process.
  const child = cp.spawnSync(
    process.execPath,
    [`${__filename}`],
    { env: { NODE_UNHANDLED_REJECTION: env } }
  );

  assert.strictEqual(child.stdout.toString(), '');
  assert(child.stderr.includes(
    `"${env}" for environment variable NODE_UNHANDLED_REJECTION`));
}

// Verify that a faulty environment variable throws on unhandled rejections.
// TODO: This should throw immediately instead of detecting the faulty value
// delayed.
process.env.NODE_UNHANDLED_REJECTION = env;

process.on('uncaughtException', common.mustCall((err, origin) => {
  common.expectsError({
    type: TypeError,
    code: 'ERR_INVALID_ENV_VALUE',
    message: 'The value "FOO" for environment variable ' +
             'NODE_UNHANDLED_REJECTION is invalid.'
  })(err);
  assert.strictEqual(origin, 'FROM_ERROR');
}));

process.on('unhandledRejection', common.mustCall(2));

assert.strictEqual(process.env.NODE_UNHANDLED_REJECTION, env);
setImmediate(() => {
  assert.strictEqual(process.env.NODE_UNHANDLED_REJECTION, undefined);
});

new Promise(() => {
  throw new Error('One');
});

Promise.reject('foo');

// Should not do anything as the default is set to warning.
global.gc();

setTimeout(common.mustCall(), 5);
