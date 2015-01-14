var tls       = require('tls');
var fs        = require('fs');
var path      = require('path');
var fork      = require('child_process').fork;
var assert    = require('assert');
var constants = require('constants');
var os        = require('os');

var async     = require('async');
var debug     = require('debug')('test-node-ssl');

var common = require('../../common');

var SSL2_COMPATIBLE_CIPHERS = 'RC4-MD5';

var CMD_LINE_OPTIONS = [ null, "--enable-ssl2", "--enable-ssl3" ];

var SERVER_SSL_PROTOCOLS = [
  null,
  'SSLv2_method', 'SSLv2_server_method',
  'SSLv3_method', 'SSLv3_server_method',
  'TLSv1_method', 'TLSv1_server_method',
  'SSLv23_method','SSLv23_server_method'
];

var CLIENT_SSL_PROTOCOLS = [
  null,
  'SSLv2_method', 'SSLv2_client_method',
  'SSLv3_method', 'SSLv3_client_method',
  'TLSv1_method', 'TLSv1_client_method',
  'SSLv23_method','SSLv23_client_method'
];

var SECURE_OPTIONS = [
  null,
  0,
  constants.SSL_OP_NO_SSLv2,
  constants.SSL_OP_NO_SSLv3,
  constants.SSL_OP_NO_SSLv2 | constants.SSL_OP_NO_SSLv3
];

function xtend(source) {
  var clone = {};

  for (var property in source) {
    if (source.hasOwnProperty(property)) {
      clone[property] = source[property];
    }
  }

  return clone;
}

function isAutoNegotiationProtocol(sslProtocol) {
  assert(sslProtocol === null || typeof sslProtocol === 'string');

  return  sslProtocol == null ||
          sslProtocol === 'SSLv23_method' ||
          sslProtocol === 'SSLv23_client_method' ||
          sslProtocol === 'SSLv23_server_method';
}

function isSameSslProtocolVersion(serverSecureProtocol, clientSecureProtocol) {
  assert(serverSecureProtocol === null || typeof serverSecureProtocol === 'string');
  assert(clientSecureProtocol === null || typeof clientSecureProtocol === 'string');

  if (serverSecureProtocol === clientSecureProtocol) {
    return true;
  }

  var serverProtocolPrefix = '';
  if (serverSecureProtocol)
    serverProtocolPrefix = serverSecureProtocol.split('_')[0];

  var clientProtocolPrefix = '';
  if (clientSecureProtocol)
    clientProtocolPrefix = clientSecureProtocol.split('_')[0];

  if (serverProtocolPrefix === clientProtocolPrefix) {
    return true;
  }

  return false;
}

function secureProtocolsCompatible(serverSecureProtocol, clientSecureProtocol) {
  if (isAutoNegotiationProtocol(serverSecureProtocol) ||
      isAutoNegotiationProtocol(clientSecureProtocol)) {
    return true;
  }

  if (isSameSslProtocolVersion(serverSecureProtocol,
                               clientSecureProtocol)) {
    return true;
  }

  return false;
}

function isSsl3Protocol(secureProtocol) {
  assert(secureProtocol === null || typeof secureProtocol === 'string');

  return secureProtocol === 'SSLv3_method' ||
         secureProtocol === 'SSLv3_client_method' ||
         secureProtocol === 'SSLv3_server_method';
}

function isSsl2Protocol(secureProtocol) {
  assert(secureProtocol === null || typeof secureProtocol === 'string');

  return secureProtocol === 'SSLv2_method' ||
         secureProtocol === 'SSLv2_client_method' ||
         secureProtocol === 'SSLv2_server_method';
}

function secureProtocolCompatibleWithSecureOptions(secureProtocol, secureOptions, cmdLineOption) {
  if (secureOptions == null) {
    if (isSsl2Protocol(secureProtocol) &&
        (!cmdLineOption || cmdLineOption.indexOf('--enable-ssl2') === -1)) {
      return false;
    }

    if (isSsl3Protocol(secureProtocol) &&
        (!cmdLineOption || cmdLineOption.indexOf('--enable-ssl3') === -1)) {
      return false;
    }
  } else {
    if (secureOptions & constants.SSL_OP_NO_SSLv2 && isSsl2Protocol(secureProtocol)) {
      return false;
    }

    if (secureOptions & constants.SSL_OP_NO_SSLv3 && isSsl3Protocol(secureProtocol)) {
      return false;
    }
  }

  return true;
}

function testSetupsCompatible(serverSetup, clientSetup) {
  debug('Determing test result for:');
  debug(serverSetup);
  debug(clientSetup);

  /*
   * If the protocols specified by the client and server are
   * not compatible (e.g SSLv2 vs SSLv3), then the test should fail.
   */
  if (!secureProtocolsCompatible(serverSetup.secureProtocol,
                                 clientSetup.secureProtocol)) {
    debug('secureProtocols not compatible! server secureProtocol: ' +
          serverSetup.secureProtocol + ', client secureProtocol: ' +
          clientSetup.secureProtocol);
    return false;
  }

  /*
   * If the client's options are not compatible with the server's protocol,
   * then the test should fail. Same if server's options are not compatible
   * with the client's protocol.
   */
  if (!secureProtocolCompatibleWithSecureOptions(serverSetup.secureProtocol,
                                                 clientSetup.secureOptions,
                                                 clientSetup.cmdLine) ||
      !secureProtocolCompatibleWithSecureOptions(clientSetup.secureProtocol,
                                                 serverSetup.secureOptions,
                                                 serverSetup.cmdLine)) {
    debug('Secure protocol not compatible with secure options!');
    return false;
  }

  var ssl2Used = isSsl2Protocol(serverSetup.secureProtocol) ||
                 isSsl2Protocol(clientSetup.secureProtocol);
  if (ssl2Used &&
      ((serverSetup.ciphers !== SSL2_COMPATIBLE_CIPHERS) ||
      (clientSetup.ciphers !== SSL2_COMPATIBLE_CIPHERS))) {
    /*
     * Default ciphers are not compatible with SSLv2. Both client *and*
     * server need to specify a SSLv2 compatible cipher to be able to use
     * SSLv2.
     */
    return false;
  }

  return true;
}

function sslSetupMakesSense(cmdLineOption, secureProtocol, secureOption) {
  if (isSsl2Protocol(secureProtocol)) {
    if (secureOption & constants.SSL_OP_NO_SSLv2 ||
        (secureOption == null && (!cmdLineOption || cmdLineOption.indexOf('--enable-ssl2') === -1))) {
      return false;
    }
  }

  if (isSsl3Protocol(secureProtocol)) {
    if (secureOption & constants.SSL_OP_NO_SSLv3 ||
        (secureOption == null && (!cmdLineOption || cmdLineOption.indexOf('--enable-ssl3') === -1))) {
      return false;
    }
  }

  return true;
}

function createTestsSetups() {

  var serversSetup = [];
  var clientsSetup = [];

  CMD_LINE_OPTIONS.forEach(function (cmdLineOption) {
    SERVER_SSL_PROTOCOLS.forEach(function (serverSecureProtocol) {
      SECURE_OPTIONS.forEach(function (secureOption) {
        if (sslSetupMakesSense(cmdLineOption,
                               serverSecureProtocol,
                               secureOption)) {
          var serverSetup = {
            cmdLine: cmdLineOption,
            secureProtocol: serverSecureProtocol,
            secureOptions: secureOption
          };

          serversSetup.push(serverSetup);

          if (isSsl2Protocol(serverSecureProtocol)) {
            var setupWithSsl2Ciphers = xtend(serverSetup);
            setupWithSsl2Ciphers.ciphers = SSL2_COMPATIBLE_CIPHERS;
            serversSetup.push(setupWithSsl2Ciphers);
          }
        }
      });
    });

    CLIENT_SSL_PROTOCOLS.forEach(function (clientSecureProtocol) {
      SECURE_OPTIONS.forEach(function (secureOption) {
        if (sslSetupMakesSense(cmdLineOption,
                               clientSecureProtocol,
                               secureOption)) {
          var clientSetup = {
            cmdLine: cmdLineOption,
            secureProtocol: clientSecureProtocol,
            secureOptions: secureOption
          };

          clientsSetup.push(clientSetup);

          if (isSsl2Protocol(clientSecureProtocol)) {
            var setupWithSsl2Ciphers = xtend(clientSetup);
            setupWithSsl2Ciphers.ciphers = SSL2_COMPATIBLE_CIPHERS;
            clientsSetup.push(setupWithSsl2Ciphers);
          }
        }
      });
    });
  });

  var testSetups = [];
  var testId = 0;
  serversSetup.forEach(function (serverSetup) {
    clientsSetup.forEach(function (clientSetup) {
      var testSetup = {
        server: serverSetup,
        client: clientSetup,
        ID: testId++
      };

      var successExpected = false;
      if (testSetupsCompatible(serverSetup, clientSetup)) {
        successExpected = true;
      }
      testSetup.successExpected = successExpected;

      testSetups.push(testSetup);
    });
  });

  return testSetups;
}

function runServer(port, secureProtocol, secureOptions, ciphers) {
  debug('Running server!');
  debug('port: ' + port);
  debug('secureProtocol: ' + secureProtocol);
  debug('secureOptions: ' + secureOptions);
  debug('ciphers: ' + ciphers);

  var keyPath = path.join(common.fixturesDir, 'agent.key');
  var certPath = path.join(common.fixturesDir, 'agent.crt');

  var key = fs.readFileSync(keyPath).toString();
  var cert = fs.readFileSync(certPath).toString();

  var server = new tls.Server({ key: key,
                                cert: cert,
                                ca: [],
                                ciphers: ciphers,
                                secureProtocol: secureProtocol,
                                secureOptions: secureOptions
                              });

  server.listen(port, function() {
    process.on('message', function onChildMsg(msg) {
      if (msg === 'close') {
        server.close();
        process.exit(0);
      }
    });

    process.send('server_listening');
  });

  server.on('error', function onServerError(err) {
    debug('Server error: ' + err);
    process.exit(1);
  });

  server.on('clientError', function onClientError(err) {
    debug('Client error on server: ' + err);
    process.exit(1);
  });
}

function runClient(port, secureProtocol, secureOptions, ciphers) {
  debug('Running client!');
  debug('port: ' + port);
  debug('secureProtocol: ' + secureProtocol);
  debug('secureOptions: ' + secureOptions);
  debug('ciphers: ' + ciphers);

  var con = tls.connect(port,
                        {
                          rejectUnauthorized: false,
                          secureProtocol: secureProtocol,
                          secureOptions: secureOptions,
                          ciphers: ciphers
                        },
                        function() {

    // TODO jgilli: test that sslProtocolUsed is at least as "secure" as
    // "secureProtocol"
    /*
     * var sslProtocolUsed = con.getVersion();
     * debug('Protocol used: ' + sslProtocolUsed);
     */

    process.send('client_done');
  });

  con.on('error', function(err) {
    debug('Client could not connect:' + err);
    process.exit(1);
  });
}

function stringToSecureOptions(secureOptionsString) {
  assert(typeof secureOptionsString === 'string');

  var secureOptions;

  var optionStrings = secureOptionsString.split('|');
  optionStrings.forEach(function (option) {
    if (option === 'SSL_OP_NO_SSLv2') {
      secureOptions |= constants.SSL_OP_NO_SSLv2;
    }

    if (option === 'SSL_OP_NO_SSLv3') {
      secureOptions |= constants.SSL_OP_NO_SSLv3;
    }

    if (option === '0') {
      secureOptions = 0;
    }
  });

  return secureOptions;
}

function processTestCmdLineOptions(argv){
  var options = {};

  argv.forEach(function (arg) {
    var key;
    var value;

    var keyValue = arg.split(':');
    var key = keyValue[0];

    if (keyValue.length == 2 && keyValue[1].length > 0) {
      value = keyValue[1];

      if (key === 'secureOptions') {
        value = stringToSecureOptions(value);
      }

      if (key === 'port') {
        value = +value;
      }
    }

    options[key] = value;
  });

  return options;
}

function checkTestExitCode(testSetup, serverExitCode, clientExitCode) {
  if (testSetup.successExpected) {
    if (serverExitCode === 0 && clientExitCode === 0) {
      debug('Test succeeded as expected!');
      return true;
    }
  } else {
    if (serverExitCode !== 0 || clientExitCode !== 0) {
      debug('Test failed as expected!');
      return true;
    }
  }

  return false;
}

function secureOptionsToString(secureOptions) {
  var secureOptsString = '';

  if (secureOptions & constants.SSL_OP_NO_SSLv2) {
    secureOptsString += 'SSL_OP_NO_SSLv2';
  }

  if (secureOptions & constants.SSL_OP_NO_SSLv3) {
    secureOptsString += '|SSL_OP_NO_SSLv3';
  }

  if (secureOptions === 0) {
    secureOptsString = '0';
  }

  return secureOptsString;
}

function forkTestProcess(processType, testSetup, port) {
  var argv = [ processType ];

  if (testSetup.secureProtocol) {
    argv.push('secureProtocol:' + testSetup.secureProtocol);
  } else {
    argv.push('secureProtocol:');
  }

  argv.push('secureOptions:' + secureOptionsToString(testSetup.secureOptions));

  if (testSetup.ciphers) {
    argv.push('ciphers:' + testSetup.ciphers);
  } else {
    argv.push('ciphers:');
  }

  argv.push('port:' + port);

  var forkOptions;
  if (testSetup.cmdLine) {
    forkOptions = {
      execArgv: [ testSetup.cmdLine ]
    }
  }

  return fork(process.argv[1],
              argv,
              forkOptions);
}

function runTest(testSetup, testDone) {
  var clientSetup = testSetup.client;
  var serverSetup = testSetup.server;

  assert(clientSetup);
  assert(serverSetup);

  debug('Starting new test on port: ' + testSetup.port);

  debug('client setup:');
  debug(clientSetup);

  debug('server setup:');
  debug(serverSetup);

  debug('Success expected:' + testSetup.successExpected);

  var serverExitCode;

  var clientStarted = false;
  var clientExitCode;

  var serverChild = forkTestProcess('server', serverSetup, testSetup.port);
  assert(serverChild);

  serverChild.on('message', function onServerMsg(msg) {
    if (msg === 'server_listening') {
      debug('Starting client!');
      clientStarted = true;

      var clientChild = forkTestProcess('client', clientSetup, testSetup.port);
      assert(clientChild);

      clientChild.on('exit', function onClientExited(exitCode) {
        debug('Client exited with code:' + exitCode);

        clientExitCode = exitCode;
        if (serverExitCode != null) {
          var err;
          if (!checkTestExitCode(testSetup, serverExitCode, clientExitCode))
            err = new Error("Test failed!");

          return testDone(err);
        } else {
          if (serverChild.connected) {
            serverChild.send('close');
          }
        }
      });

      clientChild.on('message', function onClientMsg(msg) {
        if (msg === 'client_done' && serverChild.connected) {
          serverChild.send('close');
        }
      })
    }
  });

  serverChild.on('exit', function onServerExited(exitCode) {
    debug('Server exited with code:' + exitCode);

    serverExitCode = exitCode;
    if (clientExitCode != null || !clientStarted) {
      var err;
      if (!checkTestExitCode(testSetup, serverExitCode, clientExitCode))
        err = new Error("Test failed!");

      return testDone(err);
    }
  });
}

function usage() {
  console.log('Usage: test-node-ssl [-j N] [--list-tests] [-s startIndex] ' +
              '[-e endIndex] [-o outputFile]');
  process.exit(1);
}

function processDriverCmdLineOptions(argv) {
  var options = {
    parallelTests: 1
  };

  for (var i = 1; i < argv.length; ++i) {
    if (argv[i] === '-j') {

      var nbParallelTests = +argv[i + 1];
      if (!nbParallelTests) {
        usage();
      } else {
        options.parallelTests = argv[++i];
      }
    }

    if (argv[i] === '-s') {
      var start = +argv[i + 1];
      if (!start) {
        usage();
      } else {
        options.start = argv[++i];
      }
    }

    if (argv[i] === '-e') {
      var end = +argv[i + 1];
      if (!end) {
        usage();
      } else {
        options.end = argv[++i];
      }
    }

    if (argv[i] === '--list-tests') {
      options.listTests = true;
    }

    if (argv[i] === '-o') {
      var outputFile = argv[i + 1];
      if (!outputFile) {
        usage();
      } else {
        options.outputFile = argv[++i];
      }
    }
  }

  return options;
}

function outputTestResult(test, err, output) {
  output.write(os.EOL);
  output.write('Test:' + os.EOL);
  output.write(JSON.stringify(test, null, " "));
  output.write(os.EOL);
  output.write('Result:');
  output.write(err ? 'failure' : 'success');
  output.write(os.EOL);
}

var agentType = process.argv[2];
if (agentType === 'client' || agentType === 'server') {
  var options = processTestCmdLineOptions(process.argv);
  debug('secureProtocol: ' + options.secureProtocol);
  debug('secureOptions: ' + options.secureOptions);
  debug('ciphers:' + options.ciphers);
  debug('port:' + options.port);

  if (agentType === 'client') {
    runClient(options.port,
              options.secureProtocol,
              options.secureOptions,
              options.ciphers);
  } else if (agentType === 'server') {
    runServer(options.port,
              options.secureProtocol,
              options.secureOptions,
              options.ciphers);
  }
} else {
  var driverOptions = processDriverCmdLineOptions(process.argv);
  debug('Tests driver options:');
  debug(driverOptions);
  /*
   * This is the tests driver process.
   *
   * It forks itself twice for each test. Each of the two forked processees are
   * respectfully used as an SSL client and an SSL server. The client and
   * server setup their SSL connection as generated by the "createTestsSetups"
   * function. Once both processes have exited, the tests driver process
   * compare both client and server exit codes with the expected test result
   * of the test setup. If they match, the test is successful, otherwise it
   * failed.
   */

  var testSetups = createTestsSetups();

  if (driverOptions.listTests) {
    console.log(testSetups);
    process.exit(0);
  }

  var testOutput = process.stdout;
  if (driverOptions.outputFile) {
    testOutput = fs.createWriteStream(driverOptions.outputFile)
      .on('error', function onError(err) {
        console.error(err);
        process.exit(1);
      });
  }

  debug('Tests setups:');
  debug('Number of tests: ' + testSetups.length);
  debug(JSON.stringify(testSetups, null, " "));
  debug();

  var nbTestsStarted = 0;

  function runTests(tests, callback) {
    var nbTests = tests.length;
    if (nbTests === 0) {
      return callback();
    }
    var error;
    var nbTestsDone = 0;

    debug('Starting new batch of tests...');

    var port = common.PORT;
    async.each(tests, function (test, testDone) {
      test.port = port++;

      ++nbTestsStarted;
      debug('Starting test nb: ' + nbTestsStarted);

      runTest(test, function onTestDone(err) {
        ++nbTestsDone;
        if (err && error === undefined) {
          error = new Error('Test with ID ' + test.ID + ' failed: ' + err);
        }

        outputTestResult(test, err, testOutput);

        if (nbTestsDone === nbTests)
          return testDone(error);
        return testDone();
      });

    }, function testsDone(err, results) {
      if (err) {
        assert(false,
               "At least one test in the most recent batch failed: " + err);
      }

      return callback(err);
    });
  }

  function runAllTests(allTests, allTestsDone) {
    if (allTests.length === 0) {
      return allTestsDone();
    }

    return runTests(allTests.splice(0, driverOptions.parallelTests),
                    runAllTests.bind(global, allTests, allTestsDone));
  }

  runAllTests(testSetups.slice(driverOptions.start, driverOptions.end),
              function allDone(err) {
                console.log('All tests done!');
              });
}
