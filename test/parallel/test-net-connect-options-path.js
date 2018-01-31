'use strict';
const common = require('../common');
const net = require('net');

// This file tests the option handling of net.connect,
// net.createConnect, and new Socket().connect

common.refreshTmpDir();

const CLIENT_VARIANTS = 12;

// Test connect(path)
{
  const prefix = `${common.PIPE}-net-connect-options-path`;
  const serverPath = `${prefix}-server`;
  let counter = 0;
  const server = net.createServer()
  .on('connection', common.mustCall(function(socket) {
    socket.end('ok');
  }, CLIENT_VARIANTS))
  .listen(serverPath, common.mustCall(function() {
    const getConnectCb = () => common.mustCall(function() {
      this.end();
      this.on('close', common.mustCall(function() {
        counter++;
        if (counter === CLIENT_VARIANTS) {
          server.close();
        }
      }));
    });

    // CLIENT_VARIANTS depends on the following code
    net.connect(serverPath, getConnectCb());
    net.connect(serverPath)
      .on('connect', getConnectCb());
    net.createConnection(serverPath, getConnectCb());
    net.createConnection(serverPath)
      .on('connect', getConnectCb());
    new net.Socket().connect(serverPath, getConnectCb());
    new net.Socket().connect(serverPath)
      .on('connect', getConnectCb());
    net.connect({ path: serverPath }, getConnectCb());
    net.connect({ path: serverPath })
      .on('connect', getConnectCb());
    net.createConnection({ path: serverPath }, getConnectCb());
    net.createConnection({ path: serverPath })
      .on('connect', getConnectCb());
    new net.Socket().connect({ path: serverPath }, getConnectCb());
    new net.Socket().connect({ path: serverPath })
      .on('connect', getConnectCb());
  }));
}
