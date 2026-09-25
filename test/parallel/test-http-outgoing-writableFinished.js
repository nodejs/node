'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const { Duplex } = require('stream');

// writableFinished becomes true once all data has been flushed, immediately
// before 'finish' is emitted.
{
  const server = http.createServer(common.mustCall(function(req, res) {
    assert.strictEqual(res.writableFinished, false);
    res
      .on('finish', common.mustCall(() => {
        assert.strictEqual(res.writableFinished, true);
        server.close();
      }))
      .end();
  }));

  server.listen(0);

  server.on('listening', common.mustCall(function() {
    const clientRequest = http.request({
      port: server.address().port,
      method: 'GET',
      path: '/'
    });

    assert.strictEqual(clientRequest.writableFinished, false);
    clientRequest
      .on('finish', common.mustCall(() => {
        assert.strictEqual(clientRequest.writableFinished, true);
      }))
      .end();
    assert.strictEqual(clientRequest.writableFinished, false);
  }));
}

// A request whose writes fail never becomes writableFinished and never emits
// 'finish'; the end() callback receives the write error instead.
{
  const writeError = new Error('forced write failure');
  const socket = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      callback(writeError);
    },
  });
  const failedRequest = http.request({
    createConnection: common.mustCall(() => socket),
    method: 'POST',
  });

  failedRequest.on('finish', common.mustNotCall());
  failedRequest.on('error', common.mustCall((err) => {
    assert.strictEqual(err, writeError);
  }));
  failedRequest.on('close', common.mustCall(() => {
    assert.strictEqual(failedRequest.writableFinished, false);
  }));

  failedRequest.write('body', common.mustCall((err) => {
    assert.strictEqual(err, writeError);
  }));
  failedRequest.end(common.mustCall((err) => {
    assert.ok(err instanceof Error);
    assert.strictEqual(failedRequest.writableFinished, false);

    // Ending again after the flush has failed still reports the failure.
    failedRequest.end(common.mustCall((endAgainErr) => {
      assert.strictEqual(endAgainErr, err);
    }));
  }));
}

// The same for a server response whose flush fails (e.g. the connection is
// reset mid-flush). Unlike the client case, the error here only ever
// surfaces through the socket write callbacks.
{
  const writeError = new Error('forced write failure');
  const socket = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      callback(writeError);
    },
  });

  const server = http.createServer(common.mustCall((req, res) => {
    res.on('finish', common.mustNotCall());
    res.on('close', common.mustCall(() => {
      assert.strictEqual(res.writableFinished, false);
    }));
    res.end('hello', common.mustCall((err) => {
      assert.strictEqual(err, writeError);
      assert.strictEqual(res.writableFinished, false);
    }));
  }));

  server.emit('connection', socket);
  socket.push('GET / HTTP/1.1\r\nHost: example.com\r\n\r\n');
}

// The same when end() happens after the failed write, with no data left to
// flush: the write failure must still be detected even though end() itself
// has nothing to send.
{
  const writeError = new Error('forced write failure');
  const socket = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      callback(writeError);
    },
  });

  const server = http.createServer(common.mustCall((req, res) => {
    res.on('finish', common.mustNotCall());
    res.setHeader('Content-Length', '5');
    res.write('hello', common.mustCall((err) => {
      assert.strictEqual(err, writeError);
    }));
    setImmediate(common.mustCall(() => {
      res.end(common.mustCall((err) => {
        assert.strictEqual(err, writeError);
        assert.strictEqual(res.writableFinished, false);
      }));
    }));
  }));

  server.emit('connection', socket);
  socket.push('GET / HTTP/1.1\r\nHost: example.com\r\n\r\n');
}
