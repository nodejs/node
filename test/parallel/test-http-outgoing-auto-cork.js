'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

function responseBody(response) {
  return response.slice(response.indexOf('\r\n\r\n') + 4);
}

function getRawResponse(onRequest) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      onRequest(res);
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.connect({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      let response = '';

      socket.setEncoding('latin1');
      socket.on('error', reject);
      socket.on('data', (chunk) => response += chunk);
      socket.on('end', common.mustCall(() => {
        server.close(common.mustCall(() => resolve(response)));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.write(
          'GET / HTTP/1.1\r\nHost: localhost\r\n' +
          'Connection: close\r\n\r\n',
        );
      }));
    }));
  });
}

async function testAutomaticCork() {
  const callbacks = [];
  const response = await getRawResponse(common.mustCall((res) => {
    assert.throws(
      () => res.write('ignored', 'invalid'),
      { code: 'ERR_UNKNOWN_ENCODING' },
    );
    const chunk = new Uint8Array([0x41]);
    res.write(chunk, common.mustCall(() => callbacks.push('A')));
    res.write('', common.mustCall(() => callbacks.push('empty')));
    res.end('BC', common.mustCall(() => {
      callbacks.push('end');
      assert.deepStrictEqual(callbacks, ['A', 'empty', 'end']);
    }));
  }));

  assert.strictEqual(responseBody(response), '3\r\nABC\r\n0\r\n\r\n');
}

async function testDetachedUint8Array() {
  const response = await getRawResponse(common.mustCall((res) => {
    const chunk = new Uint8Array([0x41]);
    res.write(chunk, common.mustCall());
    structuredClone(chunk.buffer, { transfer: [chunk.buffer] });
    res.end();
  }));

  assert.match(response, /^HTTP\/1\.1 200 OK\r\n/);
}

async function testExplicitCorkedEnd() {
  const response = await getRawResponse(common.mustCall((res) => {
    res.flushHeaders();
    res.cork();
    res.cork();
    res.write('D');
    res.write('E');
    res.uncork();
    res.end('F');
    assert.strictEqual(res.writableCorked, 0);
  }));

  assert.strictEqual(responseBody(response), '3\r\nDEF\r\n0\r\n\r\n');
}

async function testTickBoundary() {
  const response = await getRawResponse(common.mustCall((res) => {
    res.write('G');
    process.nextTick(() => res.end('H'));
  }));

  assert.strictEqual(
    responseBody(response),
    '1\r\nG\r\n1\r\nH\r\n0\r\n\r\n',
  );
}

function testDestroyedWrite() {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      res.write('I', common.mustCall((error) => {
        assert.strictEqual(error.code, 'ERR_STREAM_DESTROYED');
        server.close(common.mustCall(resolve));
      }));
      res.destroy();
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const req = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      req.on('error', common.mustCall((error) => {
        assert.strictEqual(error.code, 'ECONNRESET');
      }));
    }));
  });
}

async function main() {
  await testAutomaticCork();
  await testDetachedUint8Array();
  await testExplicitCorkedEnd();
  await testTickBoundary();
  await testDestroyedWrite();
}

main().then(common.mustCall());
