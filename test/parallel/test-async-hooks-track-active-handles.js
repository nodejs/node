'use strict';

require('../common');

const assert = require('assert');
const async_hooks = require('async_hooks');
const net = require('net');

const activeHandles = new Map();
async_hooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (['TCPSERVERWRAP', 'TCPWRAP'].includes(type))
      activeHandles.set(asyncId, resource);
  },
  destroy(asyncId) {
    activeHandles.delete(asyncId);
  }
}).enable();

const NUM = 8;
const connections = [];
const clients = [];
let clients_counter = 0;

const server = net.createServer(function listener(c) {
  connections.push(c);
}).listen(0, makeConnection);


function makeConnection() {
  if (clients_counter >= NUM) return;
  net.connect(server.address().port, function connected() {
    clientConnected(this);
    makeConnection();
  });
}


function clientConnected(client) {
  clients.push(client);
  if (++clients_counter >= NUM)
    checkAll();
}


function checkAll() {
  const handles = Array.from(activeHandles.values());

  clients.forEach(function(item) {
    assert.ok(handles.includes(item._handle));
    item.destroy();
  });

  connections.forEach(function(item) {
    assert.ok(handles.includes(item._handle));
    item.end();
  });

  assert.ok(handles.includes(server._handle));
  server.close();
}
