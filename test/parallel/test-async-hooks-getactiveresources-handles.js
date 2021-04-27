'use strict';

require('../common');
const assert = require('assert');
const net = require('net');
const { getActiveResources } = require('async_hooks');

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
  const handles = Object.values(getActiveResources('handles'));

  assert.ok(
    handles.length >=
    clients.length + connections.length + [server].length
  );

  clients.forEach(function(item) {
    item.destroy();
  });

  connections.forEach(function(item) {
    item.end();
  });

  server.close();
}
