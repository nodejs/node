'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const child_process = require('child_process');

const CODE = `
  const net = require('net');
  process.on('message', (message, handle) => {
    // net.Socket or net.Server
    handle instanceof net.Socket ? handle.destroy() : handle.close();
    process.send('accepted');
  })
`;

const child = child_process.fork(process.execPath, { execArgv: ['-e', CODE ] });

let tcpServer;
const sockets = [];
let accepted = 0;

child.on('message', (message) => {
  assert.strictEqual(message, 'accepted');
  if (++accepted === 2) {
    child.kill();
    tcpServer.close();
    sockets.forEach((socket) => {
      socket.destroy();
    });
  }
});

tcpServer = net.createServer(common.mustCall((socket) => {
  const setSimultaneousAccepts = socket._handle.setSimultaneousAccepts;
  if (typeof setSimultaneousAccepts === 'function') {
    socket._handle.setSimultaneousAccepts = common.mustCall((...args) => {
      const ret = setSimultaneousAccepts.call(socket._handle, ...args);
      assert.strictEqual(ret, undefined);
    });
  }
  child.send(null, socket._handle);
  sockets.push(socket);
}));
tcpServer.listen(0, common.mustCall(() => {
  net.connect(tcpServer.address().port);
  const setSimultaneousAccepts = tcpServer._handle.setSimultaneousAccepts;
  if (typeof setSimultaneousAccepts === 'function') {
    tcpServer._handle.setSimultaneousAccepts = common.mustCall((...args) => {
      const ret = setSimultaneousAccepts.call(tcpServer._handle, ...args);
      assert.strictEqual(ret, 0);
    });
  }
  child.send(null, tcpServer);
}));
