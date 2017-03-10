// child process that listens on a socket, allows testing of an EADDRINUSE condition

const common = require('../common');
const net = require('net');

common.refreshTmpDir();

const server = net.createServer().listen(common.PIPE, () => {
  console.log('child listening');
  process.send('listening');
});

function onmessage() {
  console.log('child exiting');
  server.close();
}

process.once('message', onmessage);
