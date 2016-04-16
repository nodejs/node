'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var webPort = common.PORT;
var tcpPort = webPort + 1;

var listenCount = 0;
var gotThanks = false;
var tcpLengthSeen = 0;
var bufferSize = 5 * 1024 * 1024;


/*
 * 5MB of random buffer.
 */
var buffer = Buffer.allocUnsafe(bufferSize);
for (var i = 0; i < buffer.length; i++) {
  buffer[i] = parseInt(Math.random() * 10000) % 256;
}


var web = http.Server(function(req, res) {
  web.close();

  console.log(req.headers);

  var socket = net.Stream();
  socket.connect(tcpPort);

  socket.on('connect', function() {
    console.log('socket connected');
  });

  req.pipe(socket);

  req.on('end', function() {
    res.writeHead(200);
    res.write('thanks');
    res.end();
    console.log('response with \'thanks\'');
  });

  req.connection.on('error', function(e) {
    console.log('http server-side error: ' + e.message);
    process.exit(1);
  });
});
web.listen(webPort, startClient);


var tcp = net.Server(function(s) {
  tcp.close();

  console.log('tcp server connection');

  var i = 0;

  s.on('data', function(d) {
    process.stdout.write('.');
    tcpLengthSeen += d.length;
    for (var j = 0; j < d.length; j++) {
      assert.equal(buffer[i], d[j]);
      i++;
    }
  });

  s.on('end', function() {
    console.log('tcp socket disconnect');
    s.end();
  });

  s.on('error', function(e) {
    console.log('tcp server-side error: ' + e.message);
    process.exit(1);
  });
});
tcp.listen(tcpPort, startClient);


function startClient() {
  listenCount++;
  if (listenCount < 2) return;

  console.log('Making request');

  var req = http.request({
    port: common.PORT,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', function(string) {
      assert.equal('thanks', string);
      gotThanks = true;
    });
  });
  req.write(buffer);
  req.end();
  console.error('ended request', req);
}

process.on('exit', function() {
  assert.ok(gotThanks);
  assert.equal(bufferSize, tcpLengthSeen);
});
