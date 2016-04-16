// child process that listens on a socket, allows testing of an EADDRINUSE condition

var common = require('../common');
var net = require('net');

common.refreshTmpDir();

var server = net.createServer().listen(common.PIPE, function() {
  console.log('child listening');
  process.send('listening');
});

function onmessage() {
  console.log('child exiting');
  server.close();
}

process.once('message', onmessage);
