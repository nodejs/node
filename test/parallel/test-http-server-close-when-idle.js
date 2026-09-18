'use strict';

// This tests that `server.close()` also closes the connections that are busy
// when it is called, once they go idle. Such a connection used to revert to
// being a normal keep-alive connection and stay open until `keepAliveTimeout`
// reaped it, long after the server was closed.
//
// Every server below sets `keepAliveTimeout: 0` to disable keep-alive reaping,
// so `server.close()` is the only thing that can end a connection, and every
// case fails if the connection is still around a second after its response
// completed.

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

function failUnlessClosedNow(server, message) {
  const timer = setTimeout(() => assert.fail(message), common.platformTimeout(1000));

  // Emitted once the listening handle and every connection are gone.
  server.on('close', common.mustCall(() => clearTimeout(timer)));
}

// `close()` while the request is still being handled. The connection is
// active, so the `closeIdleConnections()` inside `close()` skips it.
{
  const server = http.createServer({ keepAliveTimeout: 0 }, common.mustCall((req, res) => {
    server.close();
    res.end('ok');
    failUnlessClosedNow(server, 'connection closed during a request stayed open');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers.connection, 'close');
      res.resume();
    }));
  }));
}

// `close()` once the request has been received and the handler has returned,
// with the response still pending. The connection is neither idle nor mid-request at that moment.
{
  const server = http.createServer({ keepAliveTimeout: 0 }, common.mustCall((req, res) => {
    setTimeout(common.mustCall(() => {
      server.close();
      res.end('ok');
      failUnlessClosedNow(server, 'connection closed between request and response stayed open');
    }), common.platformTimeout(50));
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers.connection, 'close');
      res.resume();
    }));
  }));
}

// The server closes the connection itself rather than leaving that to the
// client: a raw socket ignores `Connection: close` and is still sent a FIN.
{
  const server = http.createServer({ keepAliveTimeout: 0 }, common.mustCall((req, res) => {
    server.close();
    res.end('ok');
    failUnlessClosedNow(server, 'connection of a client ignoring `Connection: close` stayed open');
  }));

  server.listen(0, common.mustCall(() => {
    const socket = net.connect(server.address().port, common.mustCall(() => {
      socket.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n');
      socket.resume();
      socket.on('end', common.mustCall(() => socket.end()));
    }));
  }));
}

// `close()` after the response headers have gone out. They advertised
// keep-alive, which cannot be taken back once written, so only the check made
// when the response finishes can close this connection.
{
  const server = http.createServer({ keepAliveTimeout: 0 }, common.mustCall((req, res) => {
    res.writeHead(200, { 'Content-Length': 2 });
    res.write('o');
    res.flushHeaders();

    setTimeout(common.mustCall(() => {
      server.close();
      res.end('k');
      failUnlessClosedNow(server, 'connection closed after its response headers stayed open');
    }), common.platformTimeout(50));
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers.connection, 'keep-alive');

      res.setEncoding('utf8');
      let body = '';
      res.on('data', (chunk) => body += chunk);
      res.on('end', common.mustCall(() => assert.strictEqual(body, 'ok')));
    }));
  }));
}
