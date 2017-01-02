'use strict';
// I hate HTTP. One way of terminating an HTTP response is to not send
// a content-length header, not send a transfer-encoding: chunked header,
// and simply terminate the TCP connection. That is identity
// transfer-encoding.
//
// This test is to be sure that the https client is handling this case
// correctly.

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');
var tls = require('tls');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};


var server = tls.Server(options, function(socket) {
  console.log('2) Server got request');
  socket.write('HTTP/1.1 200 OK\r\n' +
               'Date: Tue, 15 Feb 2011 22:14:54 GMT\r\n' +
               'Expires: -1\r\n' +
               'Cache-Control: private, max-age=0\r\n' +
               'Set-Cookie: xyz\r\n' +
               'Set-Cookie: abc\r\n' +
               'Server: gws\r\n' +
               'X-XSS-Protection: 1; mode=block\r\n' +
               'Connection: close\r\n' +
               '\r\n');

  socket.write('hello world\n');

  setTimeout(function() {
    socket.end('hello world\n');
    console.log('4) Server finished response');
  }, 100);
});

server.listen(0, common.mustCall(function() {
  console.log('1) Making Request');
  https.get({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function(res) {
    var bodyBuffer = '';

    server.close();
    console.log('3) Client got response headers.');

    assert.equal('gws', res.headers.server);

    res.setEncoding('utf8');
    res.on('data', function(s) {
      bodyBuffer += s;
    });

    res.on('end', common.mustCall(function() {
      console.log('5) Client got "end" event.');
      assert.strictEqual('hello world\nhello world\n', bodyBuffer);
    }));
  }));
}));
