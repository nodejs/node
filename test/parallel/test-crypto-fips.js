'use strict';
const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const path = require('path');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const FIPS_ENABLED = 1;
const FIPS_DISABLED = 0;
const FIPS_ERROR_STRING = 'Error: Cannot set FIPS mode';
const OPTION_ERROR_STRING = 'bad option';
const CNF_FIPS_ON = path.join(common.fixturesDir, 'openssl_fips_enabled.cnf');
const CNF_FIPS_OFF = path.join(common.fixturesDir, 'openssl_fips_disabled.cnf');
let num_children_ok = 0;

function compiledWithFips() {
  return process.config.variables.openssl_fips ? true : false;
}

function addToEnv(newVar, value) {
  const envCopy = {};
  for (const e in process.env) {
    envCopy[e] = process.env[e];
  }
  envCopy[newVar] = value;
  return envCopy;
}

function testHelper(stream, args, expectedOutput, cmd, env) {
  const fullArgs = args.concat(['-e', 'console.log(' + cmd + ')']);
  const child = spawnSync(process.execPath, fullArgs, {
    cwd: path.dirname(process.execPath),
    env: env
  });

  console.error('Spawned child [pid:' + child.pid + '] with cmd \'' +
      cmd + '\' expect %j with args \'' + args + '\'' +
      ' OPENSSL_CONF=%j', expectedOutput, env.OPENSSL_CONF);

  function childOk(child) {
    console.error('Child #' + ++num_children_ok +
        ' [pid:' + child.pid + '] OK.');
  }

  function responseHandler(buffer, expectedOutput) {
    const response = buffer.toString();
    assert.notStrictEqual(response.length, 0);
    if (FIPS_ENABLED !== expectedOutput && FIPS_DISABLED !== expectedOutput) {
      // In the case of expected errors just look for a substring.
      assert.ok(response.includes(expectedOutput));
    } else {
      // Normal path where we expect either FIPS enabled or disabled.
      assert.strictEqual(expectedOutput, Number(response));
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
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', ''));

// --enable-fips should turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

//--force-fips should turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// OpenSSL config file should be able to turn on FIPS mode
testHelper(
  'stdout',
  [`--openssl-config=${CNF_FIPS_ON}`],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").fips',
  process.env);

// OPENSSL_CONF should be able to turn on FIPS mode
testHelper(
  'stdout',
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --openssl-config option should override OPENSSL_CONF
testHelper(
  'stdout',
  [`--openssl-config=${CNF_FIPS_ON}`],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

testHelper(
  'stdout',
  [`--openssl-config=${CNF_FIPS_OFF}`],
  FIPS_DISABLED,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --enable-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips', `--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// OPENSSL_CONF should _not_ make a difference to --enable-fips
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// --force-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips', `--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// Using OPENSSL_CONF should not make a difference to --force-fips
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto should be able to turn FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = true,' +
  'require("crypto").fips)',
  process.env);

// setFipsCrypto should be able to turn FIPS mode on and off
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [],
  compiledWithFips() ? FIPS_DISABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = true,' +
  'require("crypto").fips = false,' +
  'require("crypto").fips)',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [`--openssl-config=${CNF_FIPS_OFF}`],
  compiledWithFips() ? FIPS_ENABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = true,' +
  'require("crypto").fips)',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS off
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [`--openssl-config=${CNF_FIPS_ON}`],
  compiledWithFips() ? FIPS_DISABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = false,' +
  'require("crypto").fips)',
  process.env);

// --enable-fips does not prevent use of setFipsCrypto API
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  '(require("crypto").fips = false,' +
  'require("crypto").fips)',
  process.env);

// --force-fips prevents use of setFipsCrypto API
testHelper(
  'stderr',
  ['--force-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING : OPTION_ERROR_STRING,
  'require("crypto").fips = false',
  process.env);

// --force-fips and --enable-fips order does not matter
testHelper(
  'stderr',
  ['--force-fips', '--enable-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING : OPTION_ERROR_STRING,
  'require("crypto").fips = false',
  process.env);

//--enable-fips and --force-fips order does not matter
testHelper(
  'stderr',
  ['--enable-fips', '--force-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING : OPTION_ERROR_STRING,
  'require("crypto").fips = false',
  process.env);
