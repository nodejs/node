'use strict';
const common = require('../common');
const net = require('net');

// This file tests the option handling of net.connect,
// net.createConnect, and new Socket().connect

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

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
    net.connect(serverPath, getConnectCb()).resume();
    net.connect(serverPath)
      .on('connect', getConnectCb())
      .resume();
    net.createConnection(serverPath, getConnectCb()).resume();
    net.createConnection(serverPath)
      .on('connect', getConnectCb())
      .resume();
    new net.Socket().connect(serverPath, getConnectCb()).resume();
    new net.Socket().connect(serverPath)
      .on('connect', getConnectCb())
      .resume();
    net.connect({ path: serverPath }, getConnectCb()).resume();
    net.connect({ path: serverPath })
      .on('connect', getConnectCb())
      .resume();
    net.createConnection({ path: serverPath }, getConnectCb()).resume();
    net.createConnection({ path: serverPath })
      .on('connect', getConnectCb())
      .resume();
    new net.Socket().connect({ path: serverPath }, getConnectCb()).resume();
    new net.Socket().connect({ path: serverPath })
      .on('connect', getConnectCb())
      .resume();
  }));
}
