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
      // Agent's socket reusing code is registered to process.nextTick(),
      // and will be run after this function, make sure it take effect.
      setImmediate(sendSecReq, serverPort, req.socket.localPort);
    }));
  });

  // Make the `req.socket` non drained, i.e. has some data queued to write to
  // and accept by the kernel. In Linux and Mac, we only need to call `req.end(aLargeBuffer)`.
  // However, in Windows, the mechanism of acceptance is loose, the following code is a workaround
  // for Windows.

  /**
   * https://docs.microsoft.com/en-US/troubleshoot/windows/win32/data-segment-tcp-winsock says
   *
   * Winsock uses the following rules to indicate a send completion to the application
   * (depending on how the send is invoked, the completion notification could be the
   * function returning from a blocking call, signaling an event, or calling a notification
   * function, and so forth):
   * - If the socket is still within SO_SNDBUF quota, Winsock copies the data from the application
   * send and indicates the send completion to the application.
   * - If the socket is beyond SO_SNDBUF quota and there's only one previously buffered send still
   * in the stack kernel buffer, Winsock copies the data from the application send and indicates
   * the send completion to the application.
   * - If the socket is beyond SO_SNDBUF quota and there's more than one previously buffered send
   * in the stack kernel buffer, Winsock copies the data from the application send. Winsock doesn't
   * indicate the send completion to the application until the stack completes enough sends to put
   * back the socket within SO_SNDBUF quota or only one outstanding send condition.
   */

  req.on('socket', () => {
    req.socket.on('connect', () => {
      // Print tcp send buffer information
      console.log(process.report.getReport().libuv.filter((handle) => handle.type === 'tcp'));

      const dataLargerThanTCPSendBuf = Buffer.alloc(1024 * 1024 * 64, 0);

      req.write(dataLargerThanTCPSendBuf);
      req.uncork();
      if (process.platform === 'win32') {
        assert.ok(req.socket.writableLength === 0);
      }

      req.write(dataLargerThanTCPSendBuf);
      req.uncork();
      if (process.platform === 'win32') {
        assert.ok(req.socket.writableLength === 0);
      }

      req.end(dataLargerThanTCPSendBuf);
      assert.ok(req.socket.writableLength > 0);
    });
  });
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
