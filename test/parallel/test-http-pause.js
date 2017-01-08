'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const expectedServer = 'Request Body from Client';
let resultServer = '';
const expectedClient = 'Response Body from Server';
let resultClient = '';

const server = http.createServer(function(req, res) {
  console.error('pause server request');
  req.pause();
  setTimeout(function() {
    console.error('resume server request');
    req.resume();
    req.setEncoding('utf8');
    req.on('data', function(chunk) {
      resultServer += chunk;
    });
    req.on('end', function() {
      console.error(resultServer);
      res.writeHead(200);
      res.end(expectedClient);
    });
  }, 100);
});

server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  }, function(res) {
    console.error('pause client response');
    res.pause();
    setTimeout(function() {
      console.error('resume client response');
      res.resume();
      res.on('data', function(chunk) {
        resultClient += chunk;
      });
      res.on('end', function() {
        console.error(resultClient);
        server.close();
      });
    }, 100);
  });
  req.end(expectedServer);
});

process.on('exit', function() {
  assert.strictEqual(expectedServer, resultServer);
  assert.strictEqual(expectedClient, resultClient);
});
