'use strict';
var common = require('../common');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

doTest();

// This test consists of three TLS requests --
// * The first one should result in a new connection because we don't have
//   a valid session ticket.
// * The second one should result in connection resumption because we used
//   the session ticket we saved from the first connection.
// * The third one should result in a new connection because the ticket
//   that we used has expired by now.

function doTest() {
  var assert = require('assert');
  var tls = require('tls');
  var fs = require('fs');
  var join = require('path').join;
  var spawn = require('child_process').spawn;

  var SESSION_TIMEOUT = 1;

  var keyFile = join(common.fixturesDir, 'agent.key');
  var certFile = join(common.fixturesDir, 'agent.crt');
  var key = fs.readFileSync(keyFile);
  var cert = fs.readFileSync(certFile);
  var options = {
    key: key,
    cert: cert,
    ca: [cert],
    sessionTimeout: SESSION_TIMEOUT
  };

  // We need to store a sample session ticket in the fixtures directory because
  // `s_client` behaves incorrectly if we do not pass in both the `-sess_in`
  // and the `-sess_out` flags, and the `-sess_in` argument must point to a
  // file containing a proper serialization of a session ticket.
  // To avoid a source control diff, we copy the ticket to a temporary file.

  var sessionFileName = (function() {
    var ticketFileName = 'tls-session-ticket.txt';
    var fixturesPath = join(common.fixturesDir, ticketFileName);
    var tmpPath = join(common.tmpDir, ticketFileName);
    fs.writeFileSync(tmpPath, fs.readFileSync(fixturesPath));
    return tmpPath;
  }());

  // Expects a callback -- cb(connectionType : enum ['New'|'Reused'])

  var Client = function(cb) {
    var flags = [
      's_client',
      '-connect', 'localhost:' + common.PORT,
      '-sess_in', sessionFileName,
      '-sess_out', sessionFileName
    ];
    var client = spawn(common.opensslCli, flags, {
      stdio: ['ignore', 'pipe', 'ignore']
    });

    var clientOutput = '';
    client.stdout.on('data', function(data) {
      clientOutput += data.toString();
    });
    client.on('exit', function(code) {
      var connectionType;
      var grepConnectionType = function(line) {
        var matches = line.match(/(New|Reused), /);
        if (matches) {
          connectionType = matches[1];
          return true;
        }
      };
      var lines = clientOutput.split('\n');
      if (!lines.some(grepConnectionType)) {
        throw new Error('unexpected output from openssl client');
      }
      cb(connectionType);
    });
  };

  var server = tls.createServer(options, function(cleartext) {
    cleartext.on('error', function(er) {
      if (er.code !== 'ECONNRESET')
        throw er;
    });
    cleartext.end();
  });

  server.listen(common.PORT, function() {
    Client(function(connectionType) {
      assert(connectionType === 'New');
      Client(function(connectionType) {
        assert(connectionType === 'Reused');
        setTimeout(function() {
          Client(function(connectionType) {
            assert(connectionType === 'New');
            server.close();
          });
        }, (SESSION_TIMEOUT + 1) * 1000);
      });
    });
  });
}
