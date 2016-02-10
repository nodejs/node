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
const CNF_FIPS_ON = path.join(common.fixturesDir, 'openssl_fips_enabled.cnf');
const CNF_FIPS_OFF = path.join(common.fixturesDir, 'openssl_fips_disabled.cnf');
var num_children_spawned = 0;
var num_children_ok = 0;
const child_responses = {};

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

function childOk(child) {
  console.error('Child ' +  ++num_children_ok + '/' + num_children_spawned +
      ' [pid:' + child.pid + '] OK.');
}

function testHelper(requiresFips, args, expectedOutput, cmd, env) {
  const child = spawn(process.execPath, args.concat(['-i']), {
    cwd: path.dirname(process.execPath),
    env: env
  });

  console.error('Spawned child [pid:' + child.pid + '] with cmd ' +
      cmd + ' and args \'' + args + '\'');
  num_children_spawned++;

  const response_handler = function(response, expectedOutput, expectedError) {
    // We won't always have data on both stdout and stderr.
    if (undefined !== response) {
      if (EXIT_FAILURE === expectedOutput) {
        if(-1 === response.indexOf(expectedError))
        assert.notEqual(-1, response.indexOf(expectedError));
      } else {
        assert.equal(expectedOutput, response);
      }
      childOk(child);
    }
  };

  // Buffer all data received from children on stderr/stdout.
  const response_buffer = function(child, data) {
    // Prompt and newline may occur in undefined order.
    const response = data.toString().replace(/\n|>/g, '').trim();
    if (response.length > 0) {
      child_responses[child] = response;
    }
  };

  child.stdout.on('data', function(data) {
    response_buffer(child.pid + '-stdout', data);
  });

  child.stderr.on('data', function(data) {
    response_buffer(child.pid + '-stderr', data);
  });

  // If using FIPS mode binary, or running a generic test.
  if (compiledWithFips() || !requiresFips) {
    child.stdout.on('end', function(data) {
      response_handler(child_responses[child.pid + '-stdout'],
          expectedOutput, FIPS_ERROR_STRING);
    });
  } else {
    // If using a non-FIPS binary expect a failure.
    child.stdout.on('end', function() {
      response_handler(child_responses[child.pid + '-stdout'],
          EXIT_FAILURE, FIPS_ERROR_STRING);
    });
    /* Failure to start Node executable is reported on stderr */
    child.stderr.on('end', function() {
      response_handler(child_responses[child.pid + '-stderr'],
          EXIT_FAILURE, OPTION_ERROR_STRING);
    });
  }
  // Wait for write to complete before exiting child, but we won't be able to
  // write to the child's stdin if cmd line args are rejected and process dies
  // first, resulting in EPIPE. We can avoid EPIPE by  not writing anything.
  if (null !== cmd) {
    child.stdin.setEncoding('utf-8');
    child.stdin.write(cmd + '\n;process.exit()\n');
  }
}

// By default FIPS should be off in both FIPS and non-FIPS builds.
testHelper(false,
  [],
  FIPS_DISABLED,
  'require("crypto").fips',
  process.env);

// --enable-fips should force FIPS mode on
testHelper(true,
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  process.env);

//--disable-fips should force FIPS mode on
testHelper(true,
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  process.env);

// --disable-fips should take precedence over --enable-fips
testHelper(true,
  ['--disable-fips', '--enable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  process.env);

// --disable-fips and --enable-fips order do not matter
testHelper(true,
  ['--enable-fips', '--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  process.env);

// OpenSSL config file should be able to turn on FIPS mode
testHelper(false,
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  'require("crypto").fips',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips should take precedence over OpenSSL config file
testHelper(true,
  ['--disable-fips'],
  compiledWithFips() ? FIPS_DISABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --enable-fips should take precedence over OpenSSL config file
testHelper(true,
  ['--enable-fips'],
  compiledWithFips() ? FIPS_ENABLED : EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips' : null,
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto should be able to turn FIPS mode on
testHelper(true, [], FIPS_ENABLED,
  '(require("crypto").fips = true,' +
  'require("crypto").fips)',
  process.env);

// setFipsCrypto should be able to turn FIPS mode on and off
testHelper(true,
  [],
  FIPS_DISABLED,
  '(require("crypto").fips = true,' +
  'require("crypto").fips = false,' +
  'require("crypto").fips)',
  process.env);

// setFipsCrypto takes precedence over OpenSSL config file, FIPS on
testHelper(true,
  [],
  compiledWithFips() ? FIPS_ENABLED : FIPS_DISABLED,
  '(require("crypto").fips = true,' +
  'require("crypto").fips)',
  addToEnv('OPENSSL_CONF', CNF_FIPS_OFF));

// setFipsCrypto takes precedence over OpenSSL config file, FIPS off
testHelper(true,
  [],
  FIPS_DISABLED,
  '(require("crypto").fips = false,' +
  'require("crypto").fips)',
  addToEnv('OPENSSL_CONF', CNF_FIPS_ON));

// --disable-fips prevents use of setFipsCrypto API
testHelper(true,
  ['--disable-fips'],
  EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips = true' : null,
  process.env);

// --enable-fips prevents use of setFipsCrypto API
testHelper(true,
  ['--enable-fips'],
  EXIT_FAILURE,
  compiledWithFips() ? 'require("crypto").fips = false' : null,
  process.env);

