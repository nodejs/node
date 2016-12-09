'use strict';
const common = require('../common');
// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');
var path = require('path');

function file(fname) {
  return path.resolve(common.fixturesDir, 'keys', fname);
}

function read(fname) {
  return fs.readFileSync(file(fname));
}

// key1 is signed by ca1.
var key1 = read('agent1-key.pem');
var cert1 = read('agent1-cert.pem');

// key2 has a self signed cert
var key2 = read('agent2-key.pem');
var cert2 = read('agent2-cert.pem');

// key3 is signed by ca2.
var key3 = read('agent3-key.pem');
var cert3 = read('agent3-cert.pem');

var ca1 = read('ca1-cert.pem');
var ca2 = read('ca2-cert.pem');

// different agents to use different CA lists.
// this api is beyond bad.
var agent0 = new https.Agent();
var agent1 = new https.Agent({ ca: [ca1] });
var agent2 = new https.Agent({ ca: [ca2] });
var agent3 = new https.Agent({ ca: [ca1, ca2] });

var options1 = {
  key: key1,
  cert: cert1
};

var options2 = {
  key: key2,
  cert: cert2
};

var options3 = {
  key: key3,
  cert: cert3
};

var server1 = server(options1);
var server2 = server(options2);
var server3 = server(options3);

var listenWait = 0;

server1.listen(0, listening());
server2.listen(0, listening());
server3.listen(0, listening());

var responseErrors = {};
var expectResponseCount = 0;
var responseCount = 0;
var pending = 0;


function server(options, port) {
  var s = https.createServer(options, handler);
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
  return function() {
    listenWait--;
    if (listenWait === 0) {
      allListening();
    }
  };
}

function makeReq(path, port, error, host, ca) {
  pending++;
  var options = {
    port: port,
    path: path,
    ca: ca
  };

  if (!ca) {
    options.agent = agent0;
  } else {
    if (!Array.isArray(ca)) ca = [ca];
    if (-1 !== ca.indexOf(ca1) && -1 !== ca.indexOf(ca2)) {
      options.agent = agent3;
    } else if (-1 !== ca.indexOf(ca1)) {
      options.agent = agent1;
    } else if (-1 !== ca.indexOf(ca2)) {
      options.agent = agent2;
    } else {
      options.agent = agent0;
    }
  }

  if (host) {
    options.headers = { host: host };
  }
  var req = https.get(options);
  expectResponseCount++;
  var server = port === server1.address().port ? server1
      : port === server2.address().port ? server2
      : port === server3.address().port ? server3
      : null;

  if (!server) throw new Error('invalid port: ' + port);
  server.expectCount++;

  req.on('response', function(res) {
    responseCount++;
    assert.equal(res.connection.authorizationError, error);
    responseErrors[path] = res.connection.authorizationError;
    pending--;
    if (pending === 0) {
      server1.close();
      server2.close();
      server3.close();
    }
    res.resume();
  });
}

function allListening() {
  // ok, ready to start the tests!
  const port1 = server1.address().port;
  const port2 = server2.address().port;
  const port3 = server3.address().port;

  // server1: host 'agent1', signed by ca1
  makeReq('/inv1', port1, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
  makeReq('/inv1-ca1', port1,
          'Hostname/IP doesn\'t match certificate\'s altnames: ' +
              '"Host: localhost. is not cert\'s CN: agent1"',
          null, ca1);
  makeReq('/inv1-ca1ca2', port1,
          'Hostname/IP doesn\'t match certificate\'s altnames: ' +
              '"Host: localhost. is not cert\'s CN: agent1"',
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
  makeReq('/inv3-ca2', port3,
          'Hostname/IP doesn\'t match certificate\'s altnames: ' +
              '"Host: localhost. is not cert\'s CN: agent3"',
          null, ca2);
  makeReq('/inv3-ca1ca2', port3,
          'Hostname/IP doesn\'t match certificate\'s altnames: ' +
              '"Host: localhost. is not cert\'s CN: agent3"',
          null, [ca1, ca2]);
  makeReq('/val3-ca2', port3, null, 'agent3', ca2);
  makeReq('/val3-ca1ca2', port3, null, 'agent3', [ca1, ca2]);
  makeReq('/inv3-ca1', port3,
          'UNABLE_TO_VERIFY_LEAF_SIGNATURE', 'agent1', ca1);

}

process.on('exit', function() {
  console.error(responseErrors);
  assert.equal(server1.requests.length, server1.expectCount);
  assert.equal(server2.requests.length, server2.expectCount);
  assert.equal(server3.requests.length, server3.expectCount);
  assert.equal(responseCount, expectResponseCount);
});
