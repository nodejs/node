// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('BoringSSL does not support FIPS');

const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const path = require('path');
const { spawnSyncAndAssert } = require('../common/child_process');
const { internalBinding } = require('internal/test/binding');
const { testFipsCrypto } = internalBinding('crypto');

const FIPS_ENABLED = 1;
const FIPS_DISABLED = 0;
const FIPS_ENABLE_ERROR_STRING =
  '--enable-fips requires an active OpenSSL provider named "fips"';
const FIPS_FORCE_ERROR_STRING =
  '--force-fips requires an active OpenSSL provider named "fips"';

const kNoFailure = 0;
const kGenericUserError = 1;

let num_children_ok = 0;

function sharedOpenSSL() {
  return process.config.variables.node_shared_openssl;
}

function testHelper(stream, args, expectedStatus, expectedOutput, cmd, env) {
  const fullArgs = args.concat(['-e', `console.log(${cmd})`]);
  const child = spawnSync(process.execPath, fullArgs, {
    cwd: path.dirname(process.execPath),
    env: env
  });

  console.error(
    `Spawned child [pid:${child.pid}] with cmd '${cmd}' expect %j with args '${
      args}' OPENSSL_CONF=%j`, expectedOutput, env.OPENSSL_CONF);

  function childOk(child) {
    console.error(`Child #${++num_children_ok} [pid:${child.pid}] OK.`);
  }

  function responseHandler(buffer, expectedOutput) {
    const response = buffer.toString();
    assert.notStrictEqual(response.length, 0);
    if (FIPS_ENABLED !== expectedOutput && FIPS_DISABLED !== expectedOutput) {
      // In the case of expected errors just look for a substring.
      assert.ok(response.includes(expectedOutput));
    } else {
      const getFipsValue = Number(response);
      if (!Number.isNaN(getFipsValue))
        // Normal path where we expect either FIPS enabled or disabled.
        assert.strictEqual(getFipsValue, expectedOutput);
    }
    assert.strictEqual(child.status, expectedStatus);
    childOk(child);
  }

  responseHandler(child[stream], expectedOutput);
}

// --enable-fips should raise an error if OpenSSL is not FIPS enabled.
testHelper(
  testFipsCrypto() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  testFipsCrypto() ? kNoFailure : kGenericUserError,
  testFipsCrypto() ? FIPS_ENABLED : FIPS_ENABLE_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// --force-fips should raise an error if OpenSSL is not FIPS enabled.
testHelper(
  testFipsCrypto() ? 'stdout' : 'stderr',
  ['--force-fips'],
  testFipsCrypto() ? kNoFailure : kGenericUserError,
  testFipsCrypto() ? FIPS_ENABLED : FIPS_FORCE_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// Explicit provider mode should preserve the behavior of bare --force-fips.
testHelper(
  testFipsCrypto() ? 'stdout' : 'stderr',
  ['--force-fips=provider'],
  testFipsCrypto() ? kNoFailure : kGenericUserError,
  testFipsCrypto() ? FIPS_ENABLED : FIPS_FORCE_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

{
  spawnSyncAndAssert(
    process.execPath, ['--force-fips=invalid', '-e', '0'], {
      status: 9,
      stderr: /invalid value for --force-fips; expected 'provider' or 'strict'/,
    });
}

if (hasOpenSSL(3, 4)) {
  testHelper(
    testFipsCrypto() ? 'stdout' : 'stderr',
    ['--force-fips=strict'],
    testFipsCrypto() ? kNoFailure : kGenericUserError,
    testFipsCrypto() ? FIPS_ENABLED : FIPS_FORCE_ERROR_STRING,
    'require("crypto").getFips()',
    process.env);
} else {
  spawnSyncAndAssert(
    process.execPath, ['--enable-fips-indicator-events', '-e', '0'], {
      status: 9,
      stderr: /--enable-fips-indicator-events requires OpenSSL 3\.4 or later/,
    });

  spawnSyncAndAssert(
    process.execPath, ['--force-fips=strict', '-e', '0'], {
      status: 9,
      stderr: /--force-fips=strict requires OpenSSL 3\.4 or later/,
    });
}

// By default FIPS should be off in both FIPS and non-FIPS builds
// unless Node.js was configured using --shared-openssl in
// which case it may be enabled by the system.
if (!sharedOpenSSL()) {
  testHelper(
    'stdout',
    [],
    kNoFailure,
    FIPS_DISABLED,
    'require("crypto").getFips()',
    { ...process.env, 'OPENSSL_CONF': ' ' });

  // Disabling FIPS mode should not throw after OpenSSL updates the default
  // property query.
  testHelper(
    'stdout',
    [],
    kNoFailure,
    FIPS_DISABLED,
    '(() => {' +
    'const crypto = require("crypto");' +
    'crypto.setFips(true);' +
    'require("assert").strictEqual(crypto.getFips(), 1);' +
    'crypto.setFips(false);' +
    'return crypto.getFips();' +
    '})()',
    { ...process.env, 'OPENSSL_CONF': ' ' });
}

// Toggling fips with setFips should not be allowed from a worker thread
testHelper(
  'stderr',
  [],
  kGenericUserError,
  'Calling crypto.setFips() is not supported in workers',
  'new worker_threads.Worker(\'require("crypto").setFips(true);\', { eval: true })',
  process.env);

// This should succeed whether FIPS is enabled or disabled.
const test_result = testFipsCrypto();
assert.ok(test_result === 1 || test_result === 0);
