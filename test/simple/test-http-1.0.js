var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

var body = 'hello world\n';
var server_response = '';
var client_got_eof = false;

var server = http.createServer(function(req, res) {
  assert.equal('1.0', req.httpVersion);
  assert.equal(1, req.httpVersionMajor);
  assert.equal(0, req.httpVersionMinor);
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end(body);
});
server.listen(common.PORT);

server.addListener('listening', function() {
  var c = net.createConnection(common.PORT);

  c.setEncoding('utf8');

  c.addListener('connect', function() {
    c.write('GET / HTTP/1.0\r\n\r\n');
  });

  c.addListener('data', function(chunk) {
    console.log(chunk);
    server_response += chunk;
  });

  c.addListener('end', function() {
    client_got_eof = true;
    c.end();
    server.close();
  });
});

process.addListener('exit', function() {
  var m = server_response.split('\r\n\r\n');
  assert.equal(m[1], body);
  assert.equal(true, client_got_eof);
});
