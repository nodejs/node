'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const agent = new http.Agent({
  keepAlive: true,
  maxFreeSockets: Infinity,
  maxSockets: Infinity,
  maxTotalSockets: Infinity,
});

const server = net.createServer({
  pauseOnConnect: true,
}, (sock) => {
  // Do not read anything from `sock`
  sock.pause();
  sock.write('HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: Keep-Alive\r\n\r\n');
});

server.listen(0, common.mustCall(() => {
  sendFstReq(server.address().port);
}));

function sendFstReq(serverPort) {
  const req = http.request({
    agent,
    host: '127.0.0.1',
    port: serverPort,
  }, (res) => {
    res.on('data', noop);
    res.on('end', common.mustCall(() => {
      assert.ok(req.writableEnded);
      // Agent's socket reusing code is registered to process.nextTick(),
      // and will be run after this function, make sure it take effect.
      setImmediate(sendSecReq, serverPort, req.socket.localPort);
    }));
  });

  req.on('socket', common.mustCall(() => {
    // If `req` is assigned to a socket, `req.write()` write data to the
    // socket directly and the return value indicate whether the socket
    // is non drained.
    assert.strictEqual(req.write(Buffer.alloc(1024 * 1024 * 1024, 0)), false);
    req.end();
  }));
}

function sendSecReq(serverPort, fstReqCliPort) {
  // Make the second request, which should be sent on a new socket
  // because the first socket is not drained and hence can not be reused
  const req = http.request({
    agent,
    host: '127.0.0.1',
    port: serverPort,
  }, (res) => {
    res.on('data', noop);
    res.on('end', common.mustCall(() => {
      setImmediate(sendThrReq, serverPort, req.socket.localPort);
    }));
  });

  req.on('socket', common.mustCall((sock) => {
    assert.notStrictEqual(sock.localPort, fstReqCliPort);
  }));
  req.end();
}

function sendThrReq(serverPort, secReqCliPort) {
  // Make the third request, the agent should reuse the second socket we just made
  const req = http.request({
    agent,
    host: '127.0.0.1',
    port: serverPort,
  }, noop);

  req.on('socket', common.mustCall((sock) => {
    assert.strictEqual(sock.localPort, secReqCliPort);
    process.exit(0);
  }));
}

function noop() { }
