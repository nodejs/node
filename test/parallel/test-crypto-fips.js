'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}

const EXIT_FAILURE = -1;
const FIPS_ENABLED = 1;
const FIPS_DISABLED = 0;
const FIPS_ERROR_STRING = 'Error: Cannot set FIPS mode';
const OPTION_ERROR_STRING = 'bad option';
const CNF_FIPS_ON = common.fixturesDir + '/openssl_fips_enabled.cnf';
const CNF_FIPS_OFF = common.fixturesDir + '/openssl_fips_disabled.cnf';
var num_children_spawned = 0;
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

function getResponse(data)
{
  return data.toString().replace('\n', '').replace('>', '').trim();
}

function childOk(child) {
  console.log('Child ' +  ++num_children_ok + '/' + num_children_spawned +
      ' [pid:' + child.pid + '] OK.');
}

function testHelper(requiresFips, args, expectedOutput, cmd, env) {
  const child = spawn(process.execPath, args.concat(['-i']), {
    cwd: path.dirname(process.execPath),
    env: env
  });

  console.log('Spawned child [pid:' + child.pid + '] with cmd ' +
              cmd + ' and args \'' + args + '\'');
  num_children_spawned++;

  // If using FIPS mode binary, or running a generic test.
  if (compiledWithFips() || !requiresFips) {
    child.stdin.setEncoding('utf-8');
    child.stdout.on('data', function(data) {
      // Prompt and newline may occur in undefined order.
      const response = getResponse(data);
      if (response.length > 0) {
        if (EXIT_FAILURE === expectedOutput) {
          assert.notEqual(-1, response.indexOf(FIPS_ERROR_STRING));
        } else {
          assert.equal(expectedOutput, response);
        }
        childOk(child);
      }
    });
  } else {
    // If using a non-FIPS binary, expect a failure.
    const error_handler = function(data, string) {
      const response = getResponse(data);
      if (response.length > 0) {
        assert.notEqual(-1, response.indexOf(string));
        childOk(child);
      }
    };
    child.stdout.on('data', function(data) {
      error_handler(data, FIPS_ERROR_STRING);
    });
    /* Failure to start Node executable is reported on stderr */
    child.stderr.on('data', function(data) {
      error_handler(data, OPTION_ERROR_STRING);
    });
  }
  // Wait for write to complete before exiting child, but won't be able to
  // write to child stdin if cmd line args are rejected and process dies first,
  // resulting in EPIPE, we avoid this by simply not writing anything.
  if (null !== cmd) {
    child.stdin.write(cmd + '\n;process.exit()\n');
  }
}

// By default FIPS should be off in both FIPS and non-FIPS builds.
testHelper(false,
  [],
  FIPS_DISABLED,
  'require("crypto").hasFipsCrypto()',
  process.env);

// --enable-fips should force FIPS mode on
testHelper(true,
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").hasFipsCrypto()' : null,
  process.env);

//--disable-fips should force FIPS mode on
testHelper(true,
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  'require("crypto").hasFipsCrypto()',
  process.env);

// --disable-fips should take precedence over --enable-fips
testHelper(true,
  ['--disable-fips', '--enable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").hasFipsCrypto()' : null,
  process.env);

// --disable-fips and --enable-fips order do not matter
testHelper(true,
  ['--enable-fips', '--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").hasFipsCrypto()' : null,
  process.env);

// OpenSSL config file should be able to turn on FIPS mode
testHelper(false,
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").hasFipsCrypto()',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips should take precedence over OpenSSL config file
testHelper(true,
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").hasFipsCrypto()' : null,
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --enable-fips should take precedence over OpenSSL config file
testHelper(true,
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").hasFipsCrypto()' : null,
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto should be able to turn FIPS mode on
testHelper(true, [], FIPS_ENABLED,
  '(require("crypto").setFipsCrypto(1),' +
  'require("crypto").hasFipsCrypto())',
  process.env);

// setFipsCrypto should be able to turn FIPS mode on and off
testHelper(true,
  [],
  FIPS_DISABLED,
  '(require("crypto").setFipsCrypto(1),' +
  'require("crypto").setFipsCrypto(0),' +
  'require("crypto").hasFipsCrypto())',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS on
testHelper(true,
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  '(require("crypto").setFipsCrypto(1),' +
  'require("crypto").hasFipsCrypto())',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto takes precedence over OpenSSL config file, FIPS off
testHelper(true,
  [],
  FIPS_DISABLED,
  '(require("crypto").setFipsCrypto(0),' +
  'require("crypto").hasFipsCrypto())',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips prevents use of setFipsCrypto API
testHelper(true,
  ['--disable-fips'],
  EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").setFipsCrypto(1)' : null,
  process.env);

// --enable-fips prevents use of setFipsCrypto API
testHelper(true,
  ['--enable-fips'],
  EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").setFipsCrypto(0)' : null,
  process.env);
