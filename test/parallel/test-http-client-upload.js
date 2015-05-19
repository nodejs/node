'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var sent_body = '';
var server_req_complete = false;
var client_res_complete = false;

var server = http.createServer(function(req, res) {
  assert.equal('POST', req.method);
  req.setEncoding('utf8');

  req.on('data', function(chunk) {
    console.log('server got: ' + JSON.stringify(chunk));
    sent_body += chunk;
  });

  req.on('end', function() {
    server_req_complete = true;
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('hello\n');
    res.end();
  });
});
server.listen(common.PORT);

server.on('listening', function() {
  var req = http.request({
    port: common.PORT,
    method: 'POST',
    path: '/'
  }, function(res) {
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      console.log(chunk);
    });
    res.on('end', function() {
      client_res_complete = true;
      server.close();
    });
  });

  req.write('1\n');
  req.write('2\n');
  req.write('3\n');
  req.end();

  common.error('client finished sending request');
});

process.on('exit', function() {
  assert.equal('1\n2\n3\n', sent_body);
  assert.equal(true, server_req_complete);
  assert.equal(true, client_res_complete);
});
