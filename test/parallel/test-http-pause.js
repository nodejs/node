'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var expectedServer = 'Request Body from Client';
var resultServer = '';
var expectedClient = 'Response Body from Server';
var resultClient = '';

var server = http.createServer(function(req, res) {
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

server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
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
  assert.equal(expectedServer, resultServer);
  assert.equal(expectedClient, resultClient);
});
