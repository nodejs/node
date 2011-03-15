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
var assert = require('assert');

var net = require('net');
var fs = require('fs');

var tests_run = 0;

function tlsTest(port, host, caPem, keyPem, certPem) {
  var N = 50;
  var count = 0;
  var sent_final_ping = false;

  var server = net.createServer(function(socket) {
    assert.equal(true, socket.remoteAddress !== null);
    assert.equal(true, socket.remoteAddress !== undefined);
    if (host === '127.0.0.1')
      assert.equal(socket.remoteAddress, '127.0.0.1');
    else if (host == null)
      assert.equal(socket.remoteAddress, '127.0.0.1');

    socket.setEncoding('utf8');
    socket.setNoDelay();
    socket.timeout = 0;

    socket.addListener('data', function(data) {
      var verified = socket.verifyPeer();
      var peerDN = socket.getPeerCertificate('DNstring');
      assert.equal(verified, 1);
      assert.equal(peerDN,
                   'C=UK,ST=Acknack Ltd,L=Rhys Jones,O=node.js,' +
                   'OU=Test TLS Certificate,CN=localhost');
      console.log('server got: ' + JSON.stringify(data));
      assert.equal('open', socket.readyState);
      assert.equal(true, count <= N);
      if (/PING/.exec(data)) {
        socket.write('PONG');
      }
    });

    socket.addListener('end', function() {
      assert.equal('writeOnly', socket.readyState);
      socket.end();
    });

    socket.addListener('close', function(had_error) {
      assert.equal(false, had_error);
      assert.equal('closed', socket.readyState);
      socket.server.close();
    });
  });

  server.setSecure('X509_PEM', caPem, 0, keyPem, certPem);
  server.listen(port, host);

  var client = net.createConnection(port, host);

  client.setEncoding('utf8');
  client.setSecure('X509_PEM', caPem, 0, keyPem, caPem);

  client.addListener('connect', function() {
    assert.equal('open', client.readyState);
    var verified = client.verifyPeer();
    var peerDN = client.getPeerCertificate('DNstring');
    assert.equal(verified, 1);
    assert.equal(peerDN,
                 'C=UK,ST=Acknack Ltd,L=Rhys Jones,O=node.js,' +
                 'OU=Test TLS Certificate,CN=localhost');
    client.write('PING');
  });

  client.addListener('data', function(data) {
    assert.equal('PONG', data);
    count += 1;

    console.log('client got PONG');

    if (sent_final_ping) {
      assert.equal('readOnly', client.readyState);
      return;
    } else {
      assert.equal('open', client.readyState);
    }

    if (count < N) {
      client.write('PING');
    } else {
      sent_final_ping = true;
      client.write('PING');
      client.end();
    }
  });

  client.addListener('close', function() {
    assert.equal(N + 1, count);
    assert.equal(true, sent_final_ping);
    tests_run += 1;
  });
}


var have_tls;
try {
  var dummy_server = net.createServer();
  dummy_server.setSecure();
  have_tls = true;
} catch (e) {
  have_tls = false;
}

if (have_tls) {
  var caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem');
  var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem');
  var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem');

  /* All are run at once, so run on different ports */
  tlsTest(common.PORT, 'localhost', caPem, keyPem, certPem);
  tlsTest(common.PORT + 1, null, caPem, keyPem, certPem);

  process.addListener('exit', function() {
    assert.equal(2, tests_run);
  });
} else {
  console.log('Not compiled with TLS support -- skipping test');
  process.exit(0);
}
