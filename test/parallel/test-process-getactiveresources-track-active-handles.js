'use strict';

require('../common');
const assert = require('assert');
const net = require('net');
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
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'TCPSocketWrap').length,
                     clients.length + connections.length);

  clients.forEach((item) => item.destroy());
  connections.forEach((item) => item.end());

  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'TCPServerWrap').length, 1);

  server.close();
}
