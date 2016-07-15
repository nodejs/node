'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

let connections = 0;
let dataEvents = 0;
let conn;


// Server
var server = net.createServer(function(conn) {
  connections++;
  conn.end('This was the year he fell to pieces.');

  if (connections === 5)
    server.close();
});

server.listen(0, function() {
  // Client 1
  conn = require('net').createConnection(this.address().port, 'localhost');
  conn.resume();
  conn.on('data', onDataOk);


  // Client 2
  conn = require('net').createConnection(this.address().port, 'localhost');
  conn.pause();
  conn.resume();
  conn.on('data', onDataOk);


  // Client 3
  conn = require('net').createConnection(this.address().port, 'localhost');
  conn.pause();
  conn.on('data', common.fail);
  scheduleTearDown(conn);


  // Client 4
  conn = require('net').createConnection(this.address().port, 'localhost');
  conn.resume();
  conn.pause();
  conn.resume();
  conn.on('data', onDataOk);


  // Client 5
  conn = require('net').createConnection(this.address().port, 'localhost');
  conn.resume();
  conn.resume();
  conn.pause();
  conn.on('data', common.fail);
  scheduleTearDown(conn);

  function onDataOk() {
    dataEvents++;
  }

  function scheduleTearDown(conn) {
    setTimeout(function() {
      conn.removeAllListeners('data');
      conn.resume();
    }, 100);
  }
});


// Exit sanity checks
process.on('exit', function() {
  assert.strictEqual(connections, 5);
  assert.strictEqual(dataEvents, 3);
});
