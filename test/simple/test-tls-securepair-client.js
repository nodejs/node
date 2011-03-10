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

// There is a bug with 'openssl s_server' which makes it not flush certain
// important events to stdout when done over a pipe. Therefore we skip this
// test for all openssl versions less than 1.0.0.
if (!process.versions.openssl ||
    parseInt(process.versions.openssl[0]) < 1) {
  console.error("Skipping due to old OpenSSL version.");
  process.exit(0);
}


var common = require('../common');
var join = require('path').join;
var net = require('net');
var assert = require('assert');
var fs = require('fs');
var crypto = require('crypto');
var tls = require('tls');
var spawn = require('child_process').spawn;

// FIXME: Avoid the common PORT as this test currently hits a C-level
// assertion error with node_g. The program aborts without HUPing
// the openssl s_server thus causing many tests to fail with
// EADDRINUSE.
var PORT = common.PORT + 5;

var connections = 0;

var keyfn = join(common.fixturesDir, 'agent.key');
var key = fs.readFileSync(keyfn).toString();

var certfn = join(common.fixturesDir, 'agent.crt');
var cert = fs.readFileSync(certfn).toString();

var server = spawn('openssl', ['s_server',
                               '-accept', PORT,
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


var timeout = setTimeout(function () {
  server.kill();
  process.exit(1);
}, 5000);

var gotWriteCallback = false;
var serverExitCode = -1;

server.on('exit', function(code) {
  serverExitCode = code;
  clearTimeout(timeout);
});


function startClient() {
  var s = new net.Stream();

  var sslcontext = crypto.createCredentials({key: key, cert: cert});
  sslcontext.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

  var pair = tls.createSecurePair(sslcontext, false);

  assert.ok(pair.encrypted.writable);
  assert.ok(pair.cleartext.writable);

  pair.encrypted.pipe(s);
  s.pipe(pair.encrypted);

  s.connect(PORT);

  s.on('connect', function() {
    console.log('client connected');
  });

  pair.on('secure', function() {
    console.log('client: connected+secure!');
    console.log('client pair.cleartext.getPeerCertificate(): %j',
                pair.cleartext.getPeerCertificate());
    console.log('client pair.cleartext.getCipher(): %j',
                pair.cleartext.getCipher());
    setTimeout(function() {
      pair.cleartext.write('hello\r\n', function () {
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
