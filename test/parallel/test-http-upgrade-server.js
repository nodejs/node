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

require('../common');
const assert = require('assert');

const util = require('util');
const net = require('net');
const http = require('http');


let requests_recv = 0;
let requests_sent = 0;
let request_upgradeHead = null;

function createTestServer() {
  return new testServer();
}

function testServer() {
  http.Server.call(this, () => {});

  this.on('connection', function() {
    requests_recv++;
  });

  this.on('request', function(req, res) {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('okay');
    res.end();
  });

  this.on('upgrade', function(req, socket, upgradeHead) {
    socket.write('HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
                 'Upgrade: WebSocket\r\n' +
                 'Connection: Upgrade\r\n' +
                 '\r\n\r\n');

    request_upgradeHead = upgradeHead;

    socket.on('data', function(d) {
      const data = d.toString('utf8');
      if (data === 'kill') {
        socket.end();
      } else {
        socket.write(data, 'utf8');
      }
    });
  });
}

util.inherits(testServer, http.Server);


function writeReq(socket, data, encoding) {
  requests_sent++;
  socket.write(data);
}


/*-----------------------------------------------
  connection: Upgrade with listener
-----------------------------------------------*/
function test_upgrade_with_listener() {
  const conn = net.createConnection(server.address().port);
  conn.setEncoding('utf8');
  let state = 0;

  conn.on('connect', function() {
    writeReq(conn,
             'GET / HTTP/1.1\r\n' +
             'Upgrade: WebSocket\r\n' +
             'Connection: Upgrade\r\n' +
             '\r\n' +
             'WjN}|M(6');
  });

  conn.on('data', function(data) {
    state++;

    assert.strictEqual('string', typeof data);

    if (state === 1) {
      assert.strictEqual('HTTP/1.1 101', data.substr(0, 12));
      assert.strictEqual('WjN}|M(6', request_upgradeHead.toString('utf8'));
      conn.write('test', 'utf8');
    } else if (state === 2) {
      assert.strictEqual('test', data);
      conn.write('kill', 'utf8');
    }
  });

  conn.on('end', function() {
    assert.strictEqual(2, state);
    conn.end();
    server.removeAllListeners('upgrade');
    test_upgrade_no_listener();
  });
}

/*-----------------------------------------------
  connection: Upgrade, no listener
-----------------------------------------------*/
let test_upgrade_no_listener_ended = false;

function test_upgrade_no_listener() {
  const conn = net.createConnection(server.address().port);
  conn.setEncoding('utf8');

  conn.on('connect', function() {
    writeReq(conn,
             'GET / HTTP/1.1\r\n' +
             'Upgrade: WebSocket\r\n' +
             'Connection: Upgrade\r\n' +
             '\r\n');
  });

  conn.on('end', function() {
    test_upgrade_no_listener_ended = true;
    conn.end();
  });

  conn.on('close', function() {
    test_standard_http();
  });
}

/*-----------------------------------------------
  connection: normal
-----------------------------------------------*/
function test_standard_http() {
  const conn = net.createConnection(server.address().port);
  conn.setEncoding('utf8');

  conn.on('connect', function() {
    writeReq(conn, 'GET / HTTP/1.1\r\n\r\n');
  });

  conn.once('data', function(data) {
    assert.strictEqual('string', typeof data);
    assert.strictEqual('HTTP/1.1 200', data.substr(0, 12));
    conn.end();
  });

  conn.on('close', function() {
    server.close();
  });
}


const server = createTestServer();

server.listen(0, function() {
  // All tests get chained after this:
  test_upgrade_with_listener();
});


/*-----------------------------------------------
  Fin.
-----------------------------------------------*/
process.on('exit', function() {
  assert.strictEqual(3, requests_recv);
  assert.strictEqual(3, requests_sent);
  assert.ok(test_upgrade_no_listener_ended);
});
