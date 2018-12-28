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
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

common.expectWarning(
  'Warning',
  'Setting the NODE_TLS_REJECT_UNAUTHORIZED environment variable to \'0\' ' +
  'makes TLS connections and HTTPS requests insecure by disabling ' +
  'certificate verification.'
);

const assert = require('assert');
const https = require('https');

function read(fname) {
  return fixtures.readKey(fname);
}

// key1 is signed by ca1.
const key1 = read('agent1-key.pem');
const cert1 = read('agent1-cert.pem');

// key2 has a self signed cert
const key2 = read('agent2-key.pem');
const cert2 = read('agent2-cert.pem');

// key3 is signed by ca2.
const key3 = read('agent3-key.pem');
const cert3 = read('agent3-cert.pem');

const ca1 = read('ca1-cert.pem');
const ca2 = read('ca2-cert.pem');

// different agents to use different CA lists.
// this api is beyond bad.
const agent0 = new https.Agent();
const agent1 = new https.Agent({ ca: [ca1] });
const agent2 = new https.Agent({ ca: [ca2] });
const agent3 = new https.Agent({ ca: [ca1, ca2] });

const options1 = {
  key: key1,
  cert: cert1
};

const options2 = {
  key: key2,
  cert: cert2
};

const options3 = {
  key: key3,
  cert: cert3
};

const server1 = server(options1);
const server2 = server(options2);
const server3 = server(options3);

let listenWait = 0;

server1.listen(0, listening());
server2.listen(0, listening());
server3.listen(0, listening());

const responseErrors = {};
let pending = 0;


function server(options) {
  const s = https.createServer(options, handler);
  s.requests = [];
  s.expectCount = 0;
  return s;
}

function handler(req, res) {
  this.requests.push(req.url);
  res.statusCode = 200;
  res.setHeader('foo', 'bar');
  res.end('hello, world\n');
}

function listening() {
  listenWait++;
  return () => {
    listenWait--;
    if (listenWait === 0) {
      allListening();
    }
  };
}

function makeReq(path, port, error, host, ca) {
  pending++;
  const options = { port, path, ca };

  if (!ca) {
    options.agent = agent0;
  } else {
    if (!Array.isArray(ca)) ca = [ca];
    if (ca.includes(ca1) && ca.includes(ca2)) {
      options.agent = agent3;
    } else if (ca.includes(ca1)) {
      options.agent = agent1;
    } else if (ca.includes(ca2)) {
      options.agent = agent2;
    } else {
      options.agent = agent0;
    }
  }

  if (host) {
    options.headers = { host };
  }
  const req = https.get(options);
  const server = port === server1.address().port ? server1 :
    port === server2.address().port ? server2 :
      port === server3.address().port ? server3 :
        null;
  if (!server) throw new Error(`invalid port: ${port}`);
  server.expectCount++;

  req.on('response', common.mustCall((res) => {
    assert.strictEqual(res.connection.authorizationError, error);
    responseErrors[path] = res.connection.authorizationError;
    pending--;
    if (pending === 0) {
      server1.close();
      server2.close();
      server3.close();
    }
    res.resume();
  }));
}

function allListening() {
  // ok, ready to start the tests!
  const port1 = server1.address().port;
  const port2 = server2.address().port;
  const port3 = server3.address().port;

  // server1: host 'agent1', signed by ca1
  makeReq('/inv1', port1, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
  makeReq('/inv1-ca1', port1, 'ERR_TLS_CERT_ALTNAME_INVALID',
          null, ca1);
  makeReq('/inv1-ca1ca2', port1, 'ERR_TLS_CERT_ALTNAME_INVALID',
          null, [ca1, ca2]);
  makeReq('/val1-ca1', port1, null, 'agent1', ca1);
  makeReq('/val1-ca1ca2', port1, null, 'agent1', [ca1, ca2]);
  makeReq('/inv1-ca2', port1,
          'UNABLE_TO_VERIFY_LEAF_SIGNATURE', 'agent1', ca2);

  // server2: self-signed, host = 'agent2'
  // doesn't matter that thename matches, all of these will error.
  makeReq('/inv2', port2, 'DEPTH_ZERO_SELF_SIGNED_CERT');
  makeReq('/inv2-ca1', port2, 'DEPTH_ZERO_SELF_SIGNED_CERT',
          'agent2', ca1);
  makeReq('/inv2-ca1ca2', port2, 'DEPTH_ZERO_SELF_SIGNED_CERT',
          'agent2', [ca1, ca2]);

  // server3: host 'agent3', signed by ca2
  makeReq('/inv3', port3, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
  makeReq('/inv3-ca2', port3, 'ERR_TLS_CERT_ALTNAME_INVALID', null, ca2);
  makeReq('/inv3-ca1ca2', port3, 'ERR_TLS_CERT_ALTNAME_INVALID',
          null, [ca1, ca2]);
  makeReq('/val3-ca2', port3, null, 'agent3', ca2);
  makeReq('/val3-ca1ca2', port3, null, 'agent3', [ca1, ca2]);
  makeReq('/inv3-ca1', port3,
          'UNABLE_TO_VERIFY_LEAF_SIGNATURE', 'agent1', ca1);

}

process.on('exit', () => {
  assert.strictEqual(server1.requests.length, server1.expectCount);
  assert.strictEqual(server2.requests.length, server2.expectCount);
  assert.strictEqual(server3.requests.length, server3.expectCount);
});
