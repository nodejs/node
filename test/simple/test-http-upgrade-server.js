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

var common = require('../common');
var assert = require('assert');

var util = require('util');
var net = require('net');
var http = require('http');


var requests_recv = 0;
var requests_sent = 0;
var request_upgradeHead = null;

function createTestServer() {
  return new testServer();
}

function testServer() {
  var server = this;
  http.Server.call(server, function() {});

  server.addListener('connection', function() {
    requests_recv++;
  });

  server.addListener('request', function(req, res) {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('okay');
    res.end();
  });

  server.addListener('upgrade', function(req, socket, upgradeHead) {
    socket.write('HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
                 'Upgrade: WebSocket\r\n' +
                 'Connection: Upgrade\r\n' +
                 '\r\n\r\n');

    request_upgradeHead = upgradeHead;

    socket.ondata = function(d, start, end) {
      var data = d.toString('utf8', start, end);
      if (data == 'kill') {
        socket.end();
      } else {
        socket.write(data, 'utf8');
      }
    };
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
function test_upgrade_with_listener(_server) {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  var state = 0;

  conn.addListener('connect', function() {
    writeReq(conn,
             'GET / HTTP/1.1\r\n' +
             'Upgrade: WebSocket\r\n' +
             'Connection: Upgrade\r\n' +
             '\r\n' +
             'WjN}|M(6');
  });

  conn.addListener('data', function(data) {
    state++;

    assert.equal('string', typeof data);

    if (state == 1) {
      assert.equal('HTTP/1.1 101', data.substr(0, 12));
      assert.equal('WjN}|M(6', request_upgradeHead.toString('utf8'));
      conn.write('test', 'utf8');
    } else if (state == 2) {
      assert.equal('test', data);
      conn.write('kill', 'utf8');
    }
  });

  conn.addListener('end', function() {
    assert.equal(2, state);
    conn.end();
    _server.removeAllListeners('upgrade');
    test_upgrade_no_listener();
  });
}

/*-----------------------------------------------
  connection: Upgrade, no listener
-----------------------------------------------*/
var test_upgrade_no_listener_ended = false;

function test_upgrade_no_listener() {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');

  conn.addListener('connect', function() {
    writeReq(conn,
             'GET / HTTP/1.1\r\n' +
             'Upgrade: WebSocket\r\n' +
             'Connection: Upgrade\r\n' +
             '\r\n');
  });

  conn.addListener('end', function() {
    test_upgrade_no_listener_ended = true;
    conn.end();
  });

  conn.addListener('close', function() {
    test_standard_http();
  });
}

/*-----------------------------------------------
  connection: normal
-----------------------------------------------*/
function test_standard_http() {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');

  conn.addListener('connect', function() {
    writeReq(conn, 'GET / HTTP/1.1\r\n\r\n');
  });

  conn.addListener('data', function(data) {
    assert.equal('string', typeof data);
    assert.equal('HTTP/1.1 200', data.substr(0, 12));
    conn.end();
  });

  conn.addListener('close', function() {
    server.close();
  });
}


var server = createTestServer();

server.listen(common.PORT, function() {
  // All tests get chained after this:
  test_upgrade_with_listener(server);
});


/*-----------------------------------------------
  Fin.
-----------------------------------------------*/
process.addListener('exit', function() {
  assert.equal(3, requests_recv);
  assert.equal(3, requests_sent);
  assert.ok(test_upgrade_no_listener_ended);
});
