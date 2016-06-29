'use strict';
// https://github.com/joyent/node/issues/4948

require('../common');
var http = require('http');

var reqCount = 0;
var server = http.createServer(function(serverReq, serverRes) {
  if (reqCount) {
    serverRes.end();
    server.close();
    return;
  }
  reqCount = 1;


  // normally the use case would be to call an external site
  // does not require connecting locally or to itself to fail
  var r = http.request({hostname: 'localhost',
                        port: this.address().port}, function(res) {
    // required, just needs to be in the client response somewhere
    serverRes.end();

    // required for test to fail
    res.on('data', function(data) { });

  });
  r.on('error', function(e) {});
  r.end();

  serverRes.write('some data');
}).listen(0, function() {
  // simulate a client request that closes early
  var net = require('net');

  var sock = new net.Socket();
  sock.connect(this.address().port, 'localhost');

  sock.on('connect', function() {
    sock.write('GET / HTTP/1.1\r\n\r\n');
    sock.end();
  });
});
