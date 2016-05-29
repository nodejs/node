'use strict';

require('../common');
const assert = require('assert');
const net = require('net');
const NUM = 8;
const connections = [];
const clients = [];
var clients_counter = 0;

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
  const handles = process._getActiveHandles();

  clients.forEach(function(item) {
    assert.ok(handles.indexOf(item) > -1);
    item.destroy();
  });

  connections.forEach(function(item) {
    assert.ok(handles.indexOf(item) > -1);
    item.end();
  });

  assert.ok(handles.indexOf(server) > -1);
  server.close();
}
