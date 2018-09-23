'use strict';
var common = require('../common'),
    assert = require('assert'),
    net = require('net');

var connections = 0,
    dataEvents = 0,
    conn;


// Server
var server = net.createServer(function(conn) {
  connections++;
  conn.end('This was the year he fell to pieces.');

  if (connections === 5)
    server.close();
});

server.listen(common.PORT);


// Client 1
conn = require('net').createConnection(common.PORT, 'localhost');
conn.resume();
conn.on('data', onDataOk);


// Client 2
conn = require('net').createConnection(common.PORT, 'localhost');
conn.pause();
conn.resume();
conn.on('data', onDataOk);


// Client 3
conn = require('net').createConnection(common.PORT, 'localhost');
conn.pause();
conn.on('data', onDataError);
scheduleTearDown(conn);


// Client 4
conn = require('net').createConnection(common.PORT, 'localhost');
conn.resume();
conn.pause();
conn.resume();
conn.on('data', onDataOk);


// Client 5
conn = require('net').createConnection(common.PORT, 'localhost');
conn.resume();
conn.resume();
conn.pause();
conn.on('data', onDataError);
scheduleTearDown(conn);


// Client helper functions
function onDataError() {
  assert(false);
}

function onDataOk() {
  dataEvents++;
}

function scheduleTearDown(conn) {
  setTimeout(function() {
    conn.removeAllListeners('data');
    conn.resume();
  }, 100);
}


// Exit sanity checks
process.on('exit', function() {
  assert.strictEqual(connections, 5);
  assert.strictEqual(dataEvents, 3);
});
