var common = require('../common');
var assert = require('assert');
var http = require('http');

var N = 1024;
var bytesRecieved = 0;
var server_req_complete = false;
var client_res_complete = false;

var server = http.createServer(function(req, res) {
  assert.equal('POST', req.method);

  req.addListener('data', function(chunk) {
    bytesRecieved += chunk.length;
  });

  req.addListener('end', function() {
    server_req_complete = true;
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('hello\n');
    res.end();
  });
});
server.listen(common.PORT);

server.addListener('listening', function() {
  var client = http.createClient(common.PORT);
  var req = client.request('POST', '/');

  req.write(new Buffer(N));
  req.end();

  common.error('client finished sending request');

  req.addListener('response', function(res) {
    res.setEncoding('utf8');
    res.addListener('data', function(chunk) {
      console.log(chunk);
    });
    res.addListener('end', function() {
      client_res_complete = true;
      server.close();
    });
  });
});

process.addListener('exit', function() {
  assert.equal(N, bytesRecieved);
  assert.equal(true, server_req_complete);
  assert.equal(true, client_res_complete);
});
