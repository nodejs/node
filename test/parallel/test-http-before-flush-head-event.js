'use strict';
//var common = require('../common');
var common = {};
common.PORT = 1234;
var assert = require('assert');
var http = require('http');

var testResBody = 'other stuff!\n';

var server = http.createServer(function(req, res) {
  res.on('before-flush-head', function() {
    res.setHeader('Flush-Head', 'event-was-called');
  })
  res.writeHead(200, {
    'Content-Type': 'text/plain'
  });
  res.end(testResBody);
});
server.listen(common.PORT);


server.addListener('listening', function() {
  var options = {
    port: common.PORT,
    path: '/',
    method: 'GET'
  };
  var req = http.request(options, function(res) {
    assert.ok('flush-head' in res.headers,
              'Response headers didn\'t contain the flush-head header, indicating the before-flush-head event was not called or did not allow adding headers.');
    assert.ok(res.headers['flush-head'] === 'event-was-called',
              'Response headers didn\'t contain the flush-head header with value event-was-called, indicating the before-flush-head event was not called or did not allow adding headers.');
    res.addListener('end', function() {
      server.close();
      process.exit();
    });
    res.resume();
  });
  req.end();
});
