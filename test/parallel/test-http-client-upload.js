'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(common.mustCall(function(req, res) {
  assert.equal('POST', req.method);
  req.setEncoding('utf8');

  var sent_body = '';

  req.on('data', function(chunk) {
    console.log('server got: ' + JSON.stringify(chunk));
    sent_body += chunk;
  });

  req.on('end', common.mustCall(function() {
    assert.strictEqual('1\n2\n3\n', sent_body);
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('hello\n');
    res.end();
  }));
}));
server.listen(0);

server.on('listening', common.mustCall(function() {
  var req = http.request({
    port: this.address().port,
    method: 'POST',
    path: '/'
  }, common.mustCall(function(res) {
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      console.log(chunk);
    });
    res.on('end', common.mustCall(function() {
      server.close();
    }));
  }));

  req.write('1\n');
  req.write('2\n');
  req.write('3\n');
  req.end();
}));
