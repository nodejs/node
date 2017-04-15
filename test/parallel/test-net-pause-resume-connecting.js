// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

let connections = 0;
let dataEvents = 0;
let conn;


// Server
const server = net.createServer(function(conn) {
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
  conn.on('data', common.mustNotCall());
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
  conn.on('data', common.mustNotCall());
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
