// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');

if (!common.opensslCli) {
  console.error('Skipping because node compiled without OpenSSL CLI.');
  process.exit(0);
}

// This is a rather complex test which sets up various TLS servers with node
// and connects to them using the 'openssl s_client' command line utility
// with various keys. Depending on the certificate authority and other
// parameters given to the server, the various clients are
// - rejected,
// - accepted and "unauthorized", or
// - accepted and "authorized".

var testCases =
    [{ title: 'Do not request certs. Everyone is unauthorized.',
      requestCert: false,
      rejectUnauthorized: false,
      renegotiate: false,
      CAs: ['ca1-cert'],
      clients:
       [{ name: 'agent1', shouldReject: false, shouldAuth: false },
        { name: 'agent2', shouldReject: false, shouldAuth: false },
        { name: 'agent3', shouldReject: false, shouldAuth: false },
        { name: 'nocert', shouldReject: false, shouldAuth: false }
       ]
    },

    { title: 'Allow both authed and unauthed connections with CA1',
      requestCert: true,
      rejectUnauthorized: false,
      renegotiate: false,
      CAs: ['ca1-cert'],
      clients:
       [{ name: 'agent1', shouldReject: false, shouldAuth: true },
        { name: 'agent2', shouldReject: false, shouldAuth: false },
        { name: 'agent3', shouldReject: false, shouldAuth: false },
        { name: 'nocert', shouldReject: false, shouldAuth: false }
       ]
    },

    { title: 'Do not request certs at connection. Do that later',
      requestCert: false,
      rejectUnauthorized: false,
      renegotiate: true,
      CAs: ['ca1-cert'],
      clients:
       [{ name: 'agent1', shouldReject: false, shouldAuth: true },
        { name: 'agent2', shouldReject: false, shouldAuth: false },
        { name: 'agent3', shouldReject: false, shouldAuth: false },
        { name: 'nocert', shouldReject: false, shouldAuth: false }
       ]
    },

    { title: 'Allow only authed connections with CA1',
      requestCert: true,
      rejectUnauthorized: true,
      renegotiate: false,
      CAs: ['ca1-cert'],
      clients:
       [{ name: 'agent1', shouldReject: false, shouldAuth: true },
        { name: 'agent2', shouldReject: true },
        { name: 'agent3', shouldReject: true },
        { name: 'nocert', shouldReject: true }
       ]
    },

    { title: 'Allow only authed connections with CA1 and CA2',
      requestCert: true,
      rejectUnauthorized: true,
      renegotiate: false,
      CAs: ['ca1-cert', 'ca2-cert'],
      clients:
       [{ name: 'agent1', shouldReject: false, shouldAuth: true },
        { name: 'agent2', shouldReject: true },
        { name: 'agent3', shouldReject: false, shouldAuth: true },
        { name: 'nocert', shouldReject: true }
       ]
    },


    { title: 'Allow only certs signed by CA2 but not in the CRL',
      requestCert: true,
      rejectUnauthorized: true,
      renegotiate: false,
      CAs: ['ca2-cert'],
      crl: 'ca2-crl',
      clients:
       [
        { name: 'agent1', shouldReject: true, shouldAuth: false },
        { name: 'agent2', shouldReject: true, shouldAuth: false },
        { name: 'agent3', shouldReject: false, shouldAuth: true },
        // Agent4 has a cert in the CRL.
        { name: 'agent4', shouldReject: true, shouldAuth: false },
        { name: 'nocert', shouldReject: true }
       ]
    }
    ];


var constants = require('constants');
var assert = require('assert');
var fs = require('fs');
var tls = require('tls');
var spawn = require('child_process').spawn;


function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}


function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}


var serverKey = loadPEM('agent2-key');
var serverCert = loadPEM('agent2-cert');


function runClient(options, cb) {

  // Client can connect in three ways:
  // - Self-signed cert
  // - Certificate, but not signed by CA.
  // - Certificate signed by CA.

  var args = ['s_client', '-connect', '127.0.0.1:' + common.PORT];


  console.log('  connecting with', options.name);

  switch (options.name) {
    case 'agent1':
      // Signed by CA1
      args.push('-key');
      args.push(filenamePEM('agent1-key'));
      args.push('-cert');
      args.push(filenamePEM('agent1-cert'));
      break;

    case 'agent2':
      // Self-signed
      // This is also the key-cert pair that the server will use.
      args.push('-key');
      args.push(filenamePEM('agent2-key'));
      args.push('-cert');
      args.push(filenamePEM('agent2-cert'));
      break;

    case 'agent3':
      // Signed by CA2
      args.push('-key');
      args.push(filenamePEM('agent3-key'));
      args.push('-cert');
      args.push(filenamePEM('agent3-cert'));
      break;

    case 'agent4':
      // Signed by CA2 (rejected by ca2-crl)
      args.push('-key');
      args.push(filenamePEM('agent4-key'));
      args.push('-cert');
      args.push(filenamePEM('agent4-cert'));
      break;

    case 'nocert':
      // Do not send certificate
      break;

    default:
      throw new Error('Unknown agent name');
  }

  // To test use: openssl s_client -connect localhost:8000
  var client = spawn(common.opensslCli, args);

  var out = '';

  var rejected = true;
  var authed = false;
  var goodbye = false;

  client.stdout.setEncoding('utf8');
  client.stdout.on('data', function(d) {
    out += d;

    if (!goodbye && /_unauthed/g.test(out)) {
      console.error('  * unauthed');
      goodbye = true;
      client.stdin.end('goodbye\n');
      authed = false;
      rejected = false;
    }

    if (!goodbye && /_authed/g.test(out)) {
      console.error('  * authed');
      goodbye = true;
      client.stdin.end('goodbye\n');
      authed = true;
      rejected = false;
    }
  });

  //client.stdout.pipe(process.stdout);

  client.on('exit', function(code) {
    //assert.equal(0, code, options.name +
    //      ": s_client exited with error code " + code);
    if (options.shouldReject) {
      assert.equal(true, rejected, options.name +
          ' NOT rejected, but should have been');
    } else {
      assert.equal(false, rejected, options.name +
          ' rejected, but should NOT have been');
      assert.equal(options.shouldAuth, authed);
    }

    cb();
  });
}


// Run the tests
var successfulTests = 0;
function runTest(testIndex) {
  var tcase = testCases[testIndex];
  if (!tcase) return;

  console.error("Running '%s'", tcase.title);

  var cas = tcase.CAs.map(loadPEM);

  var crl = tcase.crl ? loadPEM(tcase.crl) : null;

  var serverOptions = {
    key: serverKey,
    cert: serverCert,
    ca: cas,
    crl: crl,
    requestCert: tcase.requestCert,
    rejectUnauthorized: tcase.rejectUnauthorized
  };

  var connections = 0;

  /*
   * If renegotiating - session might be resumed and openssl won't request
   * client's certificate (probably because of bug in the openssl)
   */
  if (tcase.renegotiate) {
    serverOptions.secureOptions =
        constants.SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;
  }

  var renegotiated = false;
  var server = tls.Server(serverOptions, function handleConnection(c) {
    if (tcase.renegotiate && !renegotiated) {
      renegotiated = true;
      setTimeout(function() {
        console.error('- connected, renegotiating');
        c.write('\n_renegotiating\n');
        return c.renegotiate({
          requestCert: true,
          rejectUnauthorized: false
        }, function(err) {
          assert(!err);
          c.write('\n_renegotiated\n');
          handleConnection(c);
        });
      }, 200);
      return;
    }

    connections++;
    if (c.authorized) {
      console.error('- authed connection: ' +
                    c.getPeerCertificate().subject.CN);
      c.write('\n_authed\n');
    } else {
      console.error('- unauthed connection: %s', c.authorizationError);
      c.write('\n_unauthed\n');
    }
  });

  function runNextClient(clientIndex) {
    var options = tcase.clients[clientIndex];
    if (options) {
      runClient(options, function() {
        runNextClient(clientIndex + 1);
      });
    } else {
      server.close();
      successfulTests++;
      runTest(testIndex + 1);
    }
  }

  server.listen(common.PORT, function() {
    if (tcase.debug) {
      console.error('TLS server running on port ' + common.PORT);
    } else {
      runNextClient(0);
    }
  });
}


runTest(0);


process.on('exit', function() {
  assert.equal(successfulTests, testCases.length);
});
