'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var http = require('http');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var body = 'hello world\n';

// Try first with http server

var server_http = http.createServer(function(req, res) {
  console.log('got HTTP request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});


server_http.listen(0, function() {
  var req = http.request({
    port: this.address().port,
    rejectUnauthorized: false
  }, function(res) {
    server_http.close();
    res.resume();
  });
  // These methods should exist on the request and get passed down to the socket
  req.setNoDelay(true);
  req.setTimeout(1000, function() { });
  req.setSocketKeepAlive(true, 1000);
  req.end();
});

// Then try https server (requires functions to be
// mirroed in tls.js's CryptoStream)

var server_https = https.createServer(options, function(req, res) {
  console.log('got HTTPS request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});

server_https.listen(0, function() {
  var req = https.request({
    port: this.address().port,
    rejectUnauthorized: false
  }, function(res) {
    server_https.close();
    res.resume();
  });
  // These methods should exist on the request and get passed down to the socket
  req.setNoDelay(true);
  req.setTimeout(1000, function() { });
  req.setSocketKeepAlive(true, 1000);
  req.end();
});
