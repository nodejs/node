'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var requestGotEnd = false;
var responseGotEnd = false;

var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.write('a');

  req.on('close', function() {
    console.error('request aborted');
    requestGotEnd = true;
  });
  res.on('close', function() {
    console.error('response aborted');
    responseGotEnd = true;
  });
});
server.listen(common.PORT);

server.on('listening', function() {
  console.error('make req');
  http.get({
    port: common.PORT
  }, function(res) {
    console.error('got res');
    res.on('data', function(data) {
      console.error('destroy res');
      res.destroy();
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.ok(requestGotEnd);
  assert.ok(responseGotEnd);
});
