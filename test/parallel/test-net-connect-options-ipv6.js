'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var dns = require('dns');

if (!common.hasIPv6) {
  console.log('1..0 # Skipped: no IPv6 support');
  return;
}

var serverGotEnd = false;
var clientGotEnd = false;

dns.lookup('localhost', 6, function(err) {
  if (err) {
    console.error('Looks like IPv6 is not really supported');
    console.error(err);
    return;
  }

  var server = net.createServer({allowHalfOpen: true}, function(socket) {
    socket.resume();
    socket.on('end', function() {
      serverGotEnd = true;
    });
    socket.end();
  });

  server.listen(common.PORT, '::1', function() {
    var client = net.connect({
      host: 'localhost',
      port: common.PORT,
      family: 6,
      allowHalfOpen: true
    }, function() {
      console.error('client connect cb');
      client.resume();
      client.on('end', function() {
        clientGotEnd = true;
        setTimeout(function() {
          assert(client.writable);
          client.end();
        }, 10);
      });
      client.on('close', function() {
        server.close();
      });
    });
  });

  process.on('exit', function() {
    console.error('exit', serverGotEnd, clientGotEnd);
    assert(serverGotEnd);
    assert(clientGotEnd);
  });
});
