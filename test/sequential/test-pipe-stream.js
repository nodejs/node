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
var net = require('net');

function test(clazz, cb) {
  var have_ping = false;
  var have_pong = false;

  function check() {
    assert.ok(have_ping);
    assert.ok(have_pong);
  }

  function ping() {
    var conn = new clazz();

    conn.on('error', function(err) {
      throw err;
    });

    conn.connect(common.PIPE, function() {
      conn.write('PING', 'utf-8');
    });

    conn.on('data', function(data) {
      assert.equal(data.toString(), 'PONG');
      have_pong = true;
      conn.destroy();
    });
  }

  function pong(conn) {
    conn.on('error', function(err) {
      throw err;
    });

    conn.on('data', function(data) {
      assert.equal(data.toString(), 'PING');
      have_ping = true;
      conn.write('PONG', 'utf-8');
    });

    conn.on('close', function() {
      server.close();
    });
  }

  var timeout = setTimeout(function() {
    server.close();
  }, 2000);

  var server = net.Server();
  server.listen(common.PIPE, ping);
  server.on('connection', pong);
  server.on('close', function() {
    clearTimeout(timeout);
    check();
    cb && cb();
  });
}

test(net.Stream, function() {
  test(net.Socket);
});

