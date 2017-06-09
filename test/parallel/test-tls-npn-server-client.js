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

'use strict';
const common = require('../common');
if (!process.features.tls_npn) {
  common.skip('Skipping because node compiled without NPN feature of' +
              ' OpenSSL.');
  return;
}

const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');


function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  crl: loadPEM('ca2-crl'),
  SNICallback: function(servername, cb) {
    cb(null, tls.createSecureContext({
      key: loadPEM('agent2-key'),
      cert: loadPEM('agent2-cert'),
      crl: loadPEM('ca2-crl'),
    }));
  },
  NPNProtocols: ['a', 'b', 'c']
};

const clientsOptions = [{
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['a', 'b', 'c'],
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['c', 'b', 'e'],
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  rejectUnauthorized: false
}, {
  port: undefined,
  key: serverOptions.key,
  cert: serverOptions.cert,
  crl: serverOptions.crl,
  NPNProtocols: ['first-priority-unsupported', 'x', 'y'],
  rejectUnauthorized: false
}];

const serverResults = [];
const clientsResults = [];

const server = tls.createServer(serverOptions, function(c) {
  serverResults.push(c.npnProtocol);
});
server.listen(0, startTest);

function startTest() {
  function connectClient(options, callback) {
    options.port = server.address().port;
    const client = tls.connect(options, function() {
      clientsResults.push(client.npnProtocol);
      client.destroy();

      callback();
    });
  }

  connectClient(clientsOptions[0], function() {
    connectClient(clientsOptions[1], function() {
      connectClient(clientsOptions[2], function() {
        connectClient(clientsOptions[3], function() {
          server.close();
        });
      });
    });
  });
}

process.on('exit', function() {
  assert.strictEqual(serverResults[0], clientsResults[0]);
  assert.strictEqual(serverResults[1], clientsResults[1]);
  assert.strictEqual(serverResults[2], 'http/1.1');
  assert.strictEqual(clientsResults[2], false);
  assert.strictEqual(serverResults[3], 'first-priority-unsupported');
  assert.strictEqual(clientsResults[3], false);
});
