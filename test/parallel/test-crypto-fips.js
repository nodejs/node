// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const path = require('path');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');
const { fipsMode } = internalBinding('config');

const FIPS_ENABLED = 1;
const FIPS_DISABLED = 0;
const FIPS_ERROR_STRING =
  'Error [ERR_CRYPTO_FIPS_UNAVAILABLE]: Cannot set FIPS mode in a ' +
  'non-FIPS build.';
const FIPS_ERROR_STRING2 =
  'Error [ERR_CRYPTO_FIPS_FORCED]: Cannot set FIPS mode, it was forced with ' +
  '--force-fips at startup.';
const OPTION_ERROR_STRING = 'bad option';

const CNF_FIPS_ON = fixtures.path('openssl_fips_enabled.cnf');
const CNF_FIPS_OFF = fixtures.path('openssl_fips_disabled.cnf');

let num_children_ok = 0;

function compiledWithFips() {
  return fipsMode ? true : false;
}

function sharedOpenSSL() {
  return process.config.variables.node_shared_openssl;
}

function testHelper(stream, args, expectedOutput, cmd, env) {
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
      // Normal path where we expect either FIPS enabled or disabled.
      assert.strictEqual(Number(response), expectedOutput);
    }
    childOk(child);
  }

  responseHandler(child[stream], expectedOutput);
}

// By default FIPS should be off in both FIPS and non-FIPS builds.
testHelper(
  'stdout',
  [],
  FIPS_DISABLED,
  'require("crypto").getFips()',
  { ...process.env, 'OPENSSL_CONF': '' });

// --enable-fips should turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// --force-fips should turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// If Node was configured using --shared-openssl fips support might be
// available depending on how OpenSSL was built. If fips support is
// available the tests that toggle the fips_mode on/off using the config
// file option will succeed and return 1 instead of 0.
//
// Note that this case is different from when calling the fips setter as the
// configuration file is handled by OpenSSL, so it is not possible for us
// to try to call the fips setter, to try to detect this situation, as
// that would throw an error:
// ("Error: Cannot set FIPS mode in a non-FIPS build.").
// Due to this uncertainty the following tests are skipped when configured
// with --shared-openssl.
if (!sharedOpenSSL()) {
  // OpenSSL config file should be able to turn on FIPS mode
  testHelper(
    'stdout',
    [`--openssl-config=${CNF_FIPS_ON}`],
    compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
    'require("crypto").getFips()',
    process.env);

  // OPENSSL_CONF should be able to turn on FIPS mode
  testHelper(
    'stdout',
    [],
    compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
    'require("crypto").getFips()',
    Object.assign({}, process.env, { 'OPENSSL_CONF': CNF_FIPS_ON }));

  // --openssl-config option should override OPENSSL_CONF
  testHelper(
    'stdout',
    [`--openssl-config=${CNF_FIPS_ON}`],
    compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
    'require("crypto").getFips()',
    Object.assign({}, process.env, { 'OPENSSL_CONF': CNF_FIPS_OFF }));
}

testHelper(
  'stdout',
  [`--openssl-config=${CNF_FIPS_OFF}`],
  FIPS_DISABLED,
  'require("crypto").getFips()',
  Object.assign({}, process.env, { 'OPENSSL_CONF': CNF_FIPS_ON }));

// --enable-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips', `--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// OPENSSL_CONF should _not_ make a difference to --enable-fips
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  Object.assign({}, process.env, { 'OPENSSL_CONF': CNF_FIPS_OFF }));

// --force-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips', `--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  process.env);

// Using OPENSSL_CONF should not make a difference to --force-fips
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").getFips()',
  Object.assign({}, process.env, { 'OPENSSL_CONF': CNF_FIPS_OFF }));

// setFipsCrypto should be able to turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_ERROR_STRING,
  '(require("crypto").setFips(true),' +
  'require("crypto").getFips())',
  process.env);

// setFipsCrypto should be able to turn FIPS mode on and off
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [],
  compiledWithFips() ? FIPS_DISABLED : FIPS_ERROR_STRING,
  '(require("crypto").setFips(true),' +
  'require("crypto").setFips(false),' +
  'require("crypto").getFips())',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [`--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : FIPS_ERROR_STRING,
  '(require("crypto").setFips(true),' +
  'require("crypto").getFips())',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS off
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [`--openssl-config=${CNF_FIPS_ON}`],
  compiledWithFips() ? FIPS_DISABLED : FIPS_ERROR_STRING,
  '(require("crypto").setFips(false),' +
  'require("crypto").getFips())',
  process.env);

// --enable-fips does not prevent use of setFipsCrypto API
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  '(require("crypto").setFips(false),' +
  'require("crypto").getFips())',
  process.env);

// --force-fips prevents use of setFipsCrypto API
testHelper(
  'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING2 : OPTION_ERROR_STRING,
  'require("crypto").setFips(false)',
  process.env);

// --force-fips makes setFipsCrypto enable a no-op (FIPS stays on)
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  '(require("crypto").setFips(true),' +
  'require("crypto").getFips())',
  process.env);

// --force-fips and --enable-fips order does not matter
testHelper(
  'stderr',
  ['--force-fips', '--enable-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING2 : OPTION_ERROR_STRING,
  'require("crypto").setFips(false)',
  process.env);

// --enable-fips and --force-fips order does not matter
testHelper(
  'stderr',
  ['--enable-fips', '--force-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING2 : OPTION_ERROR_STRING,
  'require("crypto").setFips(false)',
  process.env);
