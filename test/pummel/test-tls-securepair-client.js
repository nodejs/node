'use strict';
//

var common = require('../common');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var join = require('path').join;
var net = require('net');
var assert = require('assert');
var fs = require('fs');
var tls = require('tls');
var spawn = require('child_process').spawn;

test1();

// simple/test-tls-securepair-client
function test1() {
  test('agent.key', 'agent.crt', null, test2);
}

// simple/test-tls-ext-key-usage
function test2() {
  function check(pair) {
    // "TLS Web Client Authentication"
    assert.equal(pair.cleartext.getPeerCertificate().ext_key_usage.length, 1);
    assert.equal(pair.cleartext.getPeerCertificate().ext_key_usage[0],
                 '1.3.6.1.5.5.7.3.2');
  }
  test('keys/agent4-key.pem', 'keys/agent4-cert.pem', check);
}

function test(keyfn, certfn, check, next) {
  keyfn = join(common.fixturesDir, keyfn);
  var key = fs.readFileSync(keyfn).toString();

  certfn = join(common.fixturesDir, certfn);
  var cert = fs.readFileSync(certfn).toString();

  var server = spawn(common.opensslCli, ['s_server',
                                         '-accept', common.PORT,
                                         '-cert', certfn,
                                         '-key', keyfn]);
  server.stdout.pipe(process.stdout);
  server.stderr.pipe(process.stdout);


  var state = 'WAIT-ACCEPT';

  var serverStdoutBuffer = '';
  server.stdout.setEncoding('utf8');
  server.stdout.on('data', function(s) {
    serverStdoutBuffer += s;
    console.error(state);
    switch (state) {
      case 'WAIT-ACCEPT':
        if (/ACCEPT/g.test(serverStdoutBuffer)) {
          // Give s_server half a second to start up.
          setTimeout(startClient, 500);
          state = 'WAIT-HELLO';
        }
        break;

      case 'WAIT-HELLO':
        if (/hello/g.test(serverStdoutBuffer)) {

          // End the current SSL connection and exit.
          // See s_server(1ssl).
          server.stdin.write('Q');

          state = 'WAIT-SERVER-CLOSE';
        }
        break;

      default:
        break;
    }
  });


  var timeout = setTimeout(function() {
    server.kill();
    process.exit(1);
  }, 5000);

  var gotWriteCallback = false;
  var serverExitCode = -1;

  server.on('exit', function(code) {
    serverExitCode = code;
    clearTimeout(timeout);
    if (next) next();
  });


  function startClient() {
    var s = new net.Stream();

    var sslcontext = tls.createSecureContext({key: key, cert: cert});
    sslcontext.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

    var pair = tls.createSecurePair(sslcontext, false);

    assert.ok(pair.encrypted.writable);
    assert.ok(pair.cleartext.writable);

    pair.encrypted.pipe(s);
    s.pipe(pair.encrypted);

    s.connect(common.PORT);

    s.on('connect', function() {
      console.log('client connected');
    });

    pair.on('secure', function() {
      console.log('client: connected+secure!');
      console.log('client pair.cleartext.getPeerCertificate(): %j',
                  pair.cleartext.getPeerCertificate());
      console.log('client pair.cleartext.getCipher(): %j',
                  pair.cleartext.getCipher());
      if (check) check(pair);
      setTimeout(function() {
        pair.cleartext.write('hello\r\n', function() {
          gotWriteCallback = true;
        });
      }, 500);
    });

    pair.cleartext.on('data', function(d) {
      console.log('cleartext: %s', d.toString());
    });

    s.on('close', function() {
      console.log('client close');
    });

    pair.encrypted.on('error', function(err) {
      console.log('encrypted error: ' + err);
    });

    s.on('error', function(err) {
      console.log('socket error: ' + err);
    });

    pair.on('error', function(err) {
      console.log('secure error: ' + err);
    });
  }


  process.on('exit', function() {
    assert.equal(0, serverExitCode);
    assert.equal('WAIT-SERVER-CLOSE', state);
    assert.ok(gotWriteCallback);
  });
}
