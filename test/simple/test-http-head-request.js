var common = require('../common');
var assert = require('assert');
var http = require('http');
var util = require('util');


var body = 'hello world\n';

var server = http.createServer(function(req, res) {
  common.error('req: ' + req.method);
  res.writeHead(200, {'Content-Length': body.length});
  res.end();
  server.close();
});

var gotEnd = false;

server.listen(common.PORT, function() {
  var client = http.createClient(common.PORT);
  var request = client.request('HEAD', '/');
  request.end();
  request.addListener('response', function(response) {
    common.error('response start');
    response.addListener('end', function() {
      common.error('response end');
      gotEnd = true;
    });
  });
});

process.addListener('exit', function() {
  assert.ok(gotEnd);
});
