'use strict';
const common = require('../../common');
const assert = require('assert');
const https = require('https');
const fs = require('fs');
const path = require('path');

var engine = path.join(path.dirname(module.filename),
                       'build',
                       'Release',
                       'testengine.engine');

var agentKey = fs.readFileSync('test/fixtures/keys/agent1-key.pem');
var agentCert = fs.readFileSync('test/fixtures/keys/agent1-cert.pem');
var agentCa = fs.readFileSync('test/fixtures/keys/ca1-cert.pem');

var port = common.PORT;

const serverOptions = {
  key: agentKey,
  cert: agentCert,
  ca: agentCa,
  requestCert: true,
  rejectUnauthorized: true
};

var server = https.createServer(serverOptions, (req, res) => {
  res.writeHead(200);
  res.end('hello world');
}).listen(port, common.localhostIPv4);

var clientOptions = {
  method: 'GET',
  host: common.localhostIPv4,
  port: port,
  path: '/test',
  clientCertEngine: engine,   // engine will provide key+cert
  rejectUnauthorized: false,  // prevent failing on self-signed certificates
  headers: {}
};

var req = https.request(clientOptions, common.mustCall(function(response) {
  var body = '';
  response.setEncoding('utf8');
  response.on('data', common.mustCall(function(chunk) {
    body += chunk;
  }));

  response.on('end', common.mustCall(function() {
    assert.strictEqual(body, 'hello world');
    console.log('Test passed!');
    server.close();
  }));
}));

req.end();
