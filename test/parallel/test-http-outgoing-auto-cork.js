// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { kRawWritev } = require('internal/streams/utils');

function runRawResponse(mode, expectedBody, expectedWritevParts,
                        contentLength = false, payload = 'ABC') {
  return new Promise((resolve, reject) => {
    const writevParts = [];
    const server = http.createServer(common.mustCall((req, res) => {
      const originalWritev = res.socket[kRawWritev];
      res.socket[kRawWritev] = function(chunks, callback) {
        writevParts.push(chunks.length >> 1);
        return originalWritev.call(this, chunks, callback);
      };

      if (contentLength) {
        res.setHeader('Content-Length', 3);
      }

      if (mode === 'auto') {
        res.write('A');
        res.write('B');
        res.end('C');
      } else if (mode === 'explicit') {
        res.cork();
        res.write('A');
        res.write('B');
        res.end('C');
      } else if (mode === 'socket') {
        res.socket.cork();
        res.write('A');
        res.write('B');
        res.end('C');
      } else if (mode === 'nextTick') {
        res.write('A');
        process.nextTick(() => {
          res.write('B');
          process.nextTick(() => res.end('C'));
        });
      } else if (mode === 'separate') {
        res.write('A');
        setImmediate(() => {
          res.write('B');
          setImmediate(() => res.end('C'));
        });
      } else if (mode === 'chunkedEnd') {
        res.write(payload);
        res.end();
      } else {
        res.end(payload);
      }
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      let response = '';

      socket.setEncoding('latin1');
      socket.on('error', reject);
      socket.on('data', (chunk) => response += chunk);
      socket.on('end', common.mustCall(() => {
        const body = response.slice(response.indexOf('\r\n\r\n') + 4);
        assert.strictEqual(body, expectedBody);
        assert.deepStrictEqual(writevParts, expectedWritevParts);
        server.close(common.mustCall(resolve));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.write(
          'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n',
        );
      }));
    }));
  });
}

function runDetachedUint8Array() {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      const chunk = new Uint8Array([0x41, 0x42, 0x43]);
      res.write(chunk, common.mustSucceed(() => {
        req.socket.destroy();
      }));
      structuredClone(chunk.buffer, { transfer: [chunk.buffer] });
      // The buffered view must already be normalized before detachment.
      res.end();
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      request.on('error', () => {});
      request.on('close', common.mustCall(() => {
        server.close(common.mustCall(resolve));
      }));
    }));
  });
}

function runContentLengthCallbacks() {
  return new Promise((resolve, reject) => {
    const callbacks = [];
    const server = http.createServer(common.mustCall((req, res) => {
      res.setHeader('Content-Length', 3);
      res.write('A', common.mustCall(() => callbacks.push('A')));
      res.write('B', common.mustCall(() => callbacks.push('B')));
      res.write('', common.mustCall(() => callbacks.push('empty')));
      res.end('C', common.mustCall(() => {
        callbacks.push('end');
        assert.deepStrictEqual(callbacks, ['A', 'B', 'empty', 'end']);
      }));
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      }, common.mustCall((response) => {
        response.resume();
        response.on('end', common.mustCall(() => {
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
    }));
  });
}

function runContentLengthBody(chunks, expectedBody, expectedWritevParts) {
  return new Promise((resolve, reject) => {
    const writevParts = [];
    const server = http.createServer(common.mustCall((req, res) => {
      const originalWritev = res.socket[kRawWritev];
      res.socket[kRawWritev] = function(vector, callback) {
        writevParts.push(vector.length >> 1);
        return originalWritev.call(this, vector, callback);
      };

      res.setHeader('Content-Length', expectedBody.length);
      for (let n = 0; n < chunks.length - 1; n++) {
        res.write(chunks[n][0], chunks[n][1]);
      }
      const last = chunks[chunks.length - 1];
      res.end(last[0], last[1]);
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      }, common.mustCall((response) => {
        const body = [];
        response.on('data', (chunk) => body.push(chunk));
        response.on('end', common.mustCall(() => {
          assert.deepStrictEqual(Buffer.concat(body), expectedBody);
          assert.deepStrictEqual(writevParts, expectedWritevParts);
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
    }));
  });
}

function runClientBeforeConnect(contentLength = false) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((request, response) => {
      let body = '';
      request.setEncoding('utf8');
      request.on('data', (chunk) => body += chunk);
      request.on('end', common.mustCall(() => {
        assert.strictEqual(body, 'ABC');
        response.end();
      }));
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        headers: contentLength ? { 'Content-Length': 3 } : undefined,
      }, common.mustCall((response) => {
        response.resume();
        response.on('end', common.mustCall(() => {
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
      request.on('socket', common.mustCall((socket) => {
        request.write('A');
        request.write('B');
        process.nextTick(common.mustCall(() => {
          assert.strictEqual(typeof socket.bytesWritten, 'number');
          request.end('C');
        }));
      }));
    }));
  });
}

function runClientContentLengthBody() {
  return new Promise((resolve, reject) => {
    const expected = Buffer.from([0xff, 0x00, 0x80]);
    const writevParts = [];
    const server = http.createServer(common.mustCall((request, response) => {
      const body = [];
      request.on('data', (chunk) => body.push(chunk));
      request.on('end', common.mustCall(() => {
        assert.deepStrictEqual(Buffer.concat(body), expected);
        response.end();
      }));
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        headers: { 'Content-Length': expected.length },
      }, common.mustCall((response) => {
        response.resume();
        response.on('end', common.mustCall(() => {
          assert.deepStrictEqual(writevParts, [3]);
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
      request.on('socket', common.mustCall((socket) => {
        const originalWritev = socket[kRawWritev];
        socket[kRawWritev] = function(vector, callback) {
          writevParts.push(vector.length >> 1);
          return originalWritev.call(this, vector, callback);
        };
        request.write(Buffer.from([0xff, 0x00]));
        request.end(new Uint8Array([0x80]));
      }));
    }));
  });
}

function runInvalidEncoding(explicit, chunk, encoding, prefix = null) {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      if (explicit) {
        res.cork();
      }
      if (prefix !== null) {
        res.write(prefix);
      }
      assert.throws(
        () => res.write(chunk, encoding),
        { code: 'ERR_UNKNOWN_ENCODING' },
      );
      res.end();
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const request = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      }, common.mustCall((response) => {
        response.resume();
        response.on('end', common.mustCall(() => {
          server.close(common.mustCall(resolve));
        }));
      }));
      request.on('error', reject);
    }));
  });
}

async function main() {
  await runRawResponse('auto', '3\r\nABC\r\n0\r\n\r\n', [3]);
  await runRawResponse('explicit', '3\r\nABC\r\n0\r\n\r\n', [3]);
  await runRawResponse(
    'socket',
    '1\r\nA\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n',
    [3],
  );
  await runRawResponse(
    'nextTick',
    '1\r\nA\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n',
    [1, 1, 1],
  );
  await runRawResponse(
    'separate',
    '1\r\nA\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n',
    [1, 1, 1],
  );
  await runRawResponse('auto', 'ABC', [3], true);
  await runRawResponse('explicit', 'ABC', [3], true);
  await runRawResponse('separate', 'ABC', [1, 1, 1], true);
  await runRawResponse('end', 'ABC', [1], true);
  for (const [payload, expectedWritevParts] of [
    ['A'.repeat(1024), [1]],
    ['A'.repeat(1025), [3]],
    ['\u00e9'.repeat(512), [1]],
    ['\u00e9'.repeat(513), [3]],
  ]) {
    const length = Buffer.byteLength(payload);
    const wirePayload = Buffer.from(payload).toString('latin1');
    await runRawResponse(
      'chunkedEnd',
      `${length.toString(16)}\r\n${wirePayload}\r\n0\r\n\r\n`,
      expectedWritevParts,
      false,
      payload,
    );
  }
  await runDetachedUint8Array();
  await runContentLengthCallbacks();
  await runContentLengthBody(
    [
      [Buffer.from([0xff, 0x00]), null],
      [new Uint8Array([0x80]), null],
    ],
    Buffer.from([0xff, 0x00, 0x80]),
    [3],
  );
  await runContentLengthBody(
    [['A', 'utf16le'], ['B', 'utf16le']],
    Buffer.from([0x41, 0x00, 0x42, 0x00]),
    [3],
  );
  await runClientBeforeConnect();
  await runClientBeforeConnect(true);
  await runClientContentLengthBody();
  await runInvalidEncoding(false, 'A', 'invalid');
  await runInvalidEncoding(true, 'A', 'buffer');
  await runInvalidEncoding(false, Buffer.from('A'), 'invalid');
  await runInvalidEncoding(false, '', 'invalid', 'A');
}

main().then(common.mustCall());
