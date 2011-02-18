// I hate HTTP. One way of terminating an HTTP response is to not send
// a content-length header, not send a transfer-encoding: chunked header,
// and simply terminate the TCP connection. That is identity
// transfer-encoding.
//
// This test is to be sure that the https client is handling this case
// correctly.
if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');
var fs = require('fs');


var server = net.Server(function(socket) {
  console.log('2) Server got request');
  socket.write('HTTP/1.1 200 OK\r\n' +
               'Content-Length: 200\r\n');

  // note headers are incomplete.

  setTimeout(function() {
    socket.end('Server: gws\r\n');
    console.log('4) Server finished response');
  }, 100);
});


var gotHeaders = false;
var gotEnd = false;
var bodyBuffer = '';

server.listen(common.PORT, function() {
    console.log('1) Making Request');
  var req = http.get({ port: common.PORT }, function(res) {
    server.close();
    console.log('3) Client got response headers.');

    assert.equal('gws', res.headers.server);
    gotHeaders = true;

    res.setEncoding('utf8');
    res.on('data', function(s) {
      bodyBuffer += s;
    });

    res.on('aborted', function() {
      console.log('RESPONSE ABORTED');
    });

    req.connection.on('error', function() {
      console.log('INCOMOLETE');
    });

    res.on('end', function() {
      console.log('5) Client got "end" event.');
      gotEnd = true;
    });
  });
});

process.on('exit', function() {
  assert.ok(gotHeaders);
  assert.ok(gotEnd);
  assert.equal('hello\nworld\n', bodyBuffer);
});

