'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}

const http = require('http');
const https = require('https');
const assert = require('assert');
const fs = require('fs');
const url = require('url');
const path = require('path');

var fixtures = path.resolve(__dirname, '../fixtures/keys');
var options = {
  key: fs.readFileSync(fixtures + '/agent1-key.pem'),
  cert: fs.readFileSync(fixtures + '/agent1-cert.pem')
};

var localhost = common.localhostIPv4;
var successfulRequests = 0;

https.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('ok');
}).listen(common.PORT, function() {

  // mimic passing in a URL string

  var opt = url.parse('https://' + localhost + ':' + common.PORT + '/foo');
  opt.rejectUnauthorized = false;
  http.get(opt, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  });

  // no agent / default agent

  http.get({
    rejectUnauthorized: false,
    protocol: 'https:',
    host: localhost,
    path: '/bar',
    port: common.PORT
  }, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  });

  http.request({
    rejectUnauthorized: false,
    host: localhost,
    path: '/',
    port: common.PORT,
    protocol: 'https:',
    agent: false
  }, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  }).end();

  // custom agents

  var agent = new http.Agent();
  agent.defaultPort = common.PORT;

  http.request({
    rejectUnauthorized: false,
    host: localhost,
    path: '/foo',
    protocol: 'https:',
    agent: agent
  }, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  }).end();

  var agent2 = new http.Agent();
  agent2.defaultPort = common.PORT;
  agent2.protocol = 'https:';

  http.request({
    rejectUnauthorized: false,
    host: localhost,
    path: '/',
    agent: agent2
  }, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  }).end();

  // http global agent
  http.globalAgent.protocol = 'https:';

  http.request({
    rejectUnauthorized: false,
    host: localhost,
    path: '/foo',
    port: common.PORT
  }, function(res) {
    assert.equal(res.statusCode, 200);
    successfulRequests++;
    res.resume();
  }).end();

}).unref();

process.on('exit', function() {
  assert.equal(successfulRequests, 6);
});
