'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var util = require('util');


var body = 'hello world\n';
var id = 0;

function test(headers) {
  var port = common.PORT + id++;

  var server = http.createServer(function(req, res) {
    console.error('req: %s headers: %j', req.method, headers);
    res.writeHead(200, headers);
    res.end();
    server.close();
  });

  var gotEnd = false;

  server.listen(port, function() {
    var request = http.request({
      port: port,
      method: 'HEAD',
      path: '/'
    }, function(response) {
      console.error('response start');
      response.on('end', function() {
        console.error('response end');
        gotEnd = true;
      });
      response.resume();
    });
    request.end();
  });

  process.on('exit', function() {
    assert.ok(gotEnd);
  });
}

test({
  'Transfer-Encoding': 'chunked'
});
test({
  'Content-Length': body.length
});
