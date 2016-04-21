'use strict';
const common = require('../../common');
const assert = require('assert');
const https = require('https');
const fs = require('fs');
const path = require('path');

var engine = path.dirname(module.filename) + '/build/Release/testengine.so';

var agentKey = fs.readFileSync('test/fixtures/keys/agent1-key.pem');
var agentCert = fs.readFileSync('test/fixtures/keys/agent1-cert.pem');
var agentCa = fs.readFileSync('test/fixtures/keys/ca1-cert.pem');

var port = 18020;

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
}).listen(port);

function testFailed(message)
{
  server.close();
  assert(false, message);
}

function testPassed()
{
  console.log('Test passed!');
  server.close();
}

var clientOptions = {
  method: 'GET',
  host: '127.0.0.1',
  port: port,
  path: '/test',
//  key: agentKey,
//  cert: agentCert,
  clientCertEngine: engine,   // engine will provide key+cert
  rejectUnauthorized: false,  // prevent failing on self-signed certificates
  headers: {}
};

var req = https.request(clientOptions, (response) => {
  var body = '';
  response.setEncoding('utf8');
  response.on('data', (chunk) => {
    body += chunk;
  });

  response.on('end', () => {
    if (body == 'hello world') {
      testPassed();
    } else {
      testFailed('unexpected body: <' + body + '>');
    }
  });
});

req.on('error', (e) => {
  testFailed('request error: ' + e.message);
});

req.end();
