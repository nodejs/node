'use strict';
var common = require('../common');
var assert = require('assert');
var spawnSync = require('child_process').spawnSync;
var path = require('path');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const FIPS_ENABLED = 1;
const FIPS_DISABLED = 0;
const FIPS_ERROR_STRING = 'Error: Cannot set FIPS mode';
const OPTION_ERROR_STRING = 'bad option';
const CNF_FIPS_ON = path.join(common.fixturesDir, 'openssl_fips_enabled.cnf');
const CNF_FIPS_OFF = path.join(common.fixturesDir, 'openssl_fips_disabled.cnf');
var num_children_ok = 0;

function compiledWithFips() {
  return process.config.variables.openssl_fips ? true : false;
}

function addToEnv(newVar, value) {
  var envCopy = {};
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

  console.error('Spawned child [pid:' + child.pid + '] with cmd ' +
      cmd + ' and args \'' + args + '\'');

  function childOk(child) {
    console.error('Child #' +  ++num_children_ok +
        ' [pid:' + child.pid + '] OK.');
  }

  function responseHandler(buffer, expectedOutput) {
    const response = buffer.toString();
    // We won't always have data on both stdout and stderr.
    if (response.length > 0) {
      if (FIPS_ENABLED !== expectedOutput && FIPS_DISABLED !== expectedOutput) {
        // In the case of expected errors just look for a substring.
        assert.notEqual(-1, response.indexOf(expectedOutput));
      } else {
        // Normal path where we expect either FIPS enabled or disabled.
        assert.equal(expectedOutput, response);
      }
      childOk(child);
    }
  }

  responseHandler(child[stream], expectedOutput);
}

// By default FIPS should be off in both FIPS and non-FIPS builds.
testHelper(
  'stdout',
  [],
  FIPS_DISABLED,
  'require("crypto").fips',
  process.env);

// --enable-fips should force FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

//--disable-fips should force FIPS mode on
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// --disable-fips should take precedence over --enable-fips
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--disable-fips', '--enable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// --disable-fips and --enable-fips order do not matter
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips', '--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  process.env);

// OpenSSL config file should be able to turn on FIPS mode
testHelper(
  'stdout',
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : OPTION_ERROR_STRING,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --enable-fips should take precedence over OpenSSL config file
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  ['--enable-fips'],
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
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = true,' +
  'require("crypto").fips)',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto takes precedence over OpenSSL config file, FIPS off
testHelper(
  compiledWithFips() ? 'stdout' : 'stderr',
  [],
  compiledWithFips() ? FIPS_DISABLED : FIPS_ERROR_STRING,
  '(require("crypto").fips = false,' +
  'require("crypto").fips)',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips prevents use of setFipsCrypto API
testHelper(
  'stderr',
  ['--disable-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING : OPTION_ERROR_STRING,
  'require("crypto").fips = true',
  process.env);

// --enable-fips prevents use of setFipsCrypto API
testHelper(
  'stderr',
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ERROR_STRING : OPTION_ERROR_STRING,
  'require("crypto").fips = false',
  process.env);

