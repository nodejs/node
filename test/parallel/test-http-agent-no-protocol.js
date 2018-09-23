'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var request = 0;
var response = 0;
process.on('exit', function() {
  assert.equal(1, request, 'http server "request" callback was not called');
  assert.equal(1, response, 'http client "response" callback was not called');
});

var server = http.createServer(function(req, res) {
  res.end();
  request++;
}).listen(common.PORT, '127.0.0.1', function() {
  var opts = url.parse('http://127.0.0.1:' + common.PORT + '/');

  // remove the `protocol` field… the `http` module should fall back
  // to "http:", as defined by the global, default `http.Agent` instance.
  opts.agent = new http.Agent();
  opts.agent.protocol = null;

  http.get(opts, function(res) {
    response++;
    res.resume();
    server.close();
  });
});
