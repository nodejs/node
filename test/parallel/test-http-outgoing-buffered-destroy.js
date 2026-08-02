'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

function runDestroy(explicit, contentLength = false) {
  return new Promise((resolve, reject) => {
    const expected = new Error('intentional');
    let callbacks = 0;
    const timer = setTimeout(
      common.mustNotCall('timed out waiting for buffered write callbacks'),
      common.platformTimeout(1000),
    );
    const server = http.createServer(common.mustCall((req, res) => {
      if (explicit) {
        res.cork();
      }
      if (contentLength) {
        res.setHeader('Content-Length', 2);
      }

      function onWrite(error) {
        assert.strictEqual(error, expected);
        if (++callbacks === 2) {
          clearTimeout(timer);
          server.close(common.mustCall(resolve));
        }
      }

      res.write('A', common.mustCall(onWrite));
      res.write('B', common.mustCall(onWrite));
      res.destroy(expected);
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
        agent: false,
      });
      request.on('error', () => {});
    }));
  });
}

function runClientDestroy(waitForSocket) {
  return new Promise((resolve, reject) => {
    const expected = new Error('intentional client destroy');
    const server = http.createServer((request) => request.destroy());

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        agent: false,
      });
      request.on('error', () => {});

      function destroy() {
        if (!waitForSocket) {
          assert.strictEqual(request.socket, null);
          request.cork();
          request.write('A');
        }
        request.write('B', common.mustCall((error) => {
          assert.strictEqual(error, expected);
          server.close(common.mustCall(resolve));
        }));
        request.destroy(expected);
      }

      if (waitForSocket) {
        request.on('socket', common.mustCall(destroy));
      } else {
        destroy();
      }
    }));
  });
}

function runServerSocketDestroy(explicit, expected) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      let callbackCalled = false;
      if (explicit) {
        res.cork();
      }
      if (expected) {
        res.socket.prependOnceListener('error', common.mustCall((error) => {
          assert.strictEqual(error, expected);
          assert.strictEqual(callbackCalled, true);
        }));
      }
      res.write('A', common.mustCall((error) => {
        callbackCalled = true;
        if (!expected) {
          assert.strictEqual(error?.code, 'ERR_STREAM_DESTROYED');
        } else {
          assert.strictEqual(error, expected);
        }
        server.close(common.mustCall(resolve));
      }));
      res.socket.destroy(expected);
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
        agent: false,
      });
      request.on('error', () => {});
    }));
  });
}

function runClientSocketDestroy(explicit, expected) {
  return new Promise((resolve, reject) => {
    const server = net.createServer((socket) => socket.resume());

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        agent: false,
      });
      request.on('error', () => {});
      request.on('socket', common.mustCall((socket) => {
        socket.once('connect', common.mustCall(() => {
          let callbackCalled = false;
          if (explicit) {
            request.cork();
          }
          if (expected) {
            socket.prependOnceListener('error', common.mustCall((error) => {
              assert.strictEqual(error, expected);
              assert.strictEqual(callbackCalled, true);
            }));
          }
          request.write('A', common.mustCall((error) => {
            callbackCalled = true;
            if (!expected) {
              assert.strictEqual(error?.code, 'ERR_STREAM_DESTROYED');
            } else {
              assert.strictEqual(error, expected);
            }
            server.close(common.mustCall(resolve));
          }));
          socket.destroy(expected);
        }));
      }));
    }));
  });
}

async function main() {
  await runDestroy(false);
  await runDestroy(true);
  await runDestroy(false, true);
  await runDestroy(true, true);
  await runClientDestroy(false);
  await runClientDestroy(true);
  await runServerSocketDestroy(false);
  await runServerSocketDestroy(true);
  await runServerSocketDestroy(false, new Error('server socket destroy'));
  await runServerSocketDestroy(true, new Error('server socket destroy'));
  await runClientSocketDestroy(false);
  await runClientSocketDestroy(true);
  await runClientSocketDestroy(false, new Error('client socket destroy'));
  await runClientSocketDestroy(true, new Error('client socket destroy'));
}

main().then(common.mustCall());
