/* eslint-disable node-core/crypto-check */

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

function runRoundTrip(transport, serverOptions, requestOptions = {}) {
  return new Promise((resolve, reject) => {
    const server = serverOptions === undefined ?
      transport.createServer(onRequest) :
      transport.createServer(serverOptions, onRequest);

    function onRequest(req, res) {
      let body = '';
      req.setEncoding('utf8');
      req.on('data', (chunk) => body += chunk);
      req.on('end', common.mustCall(() => {
        assert.strictEqual(body, 'ABCD');

        const callbacks = [];
        res.setHeader('Trailer', 'x-test');
        res.flushHeaders();

        // Exercise an explicit flush while the socket remains corked.
        res.cork();
        res.cork();
        res.write('E', common.mustCall(() => callbacks.push('E')));
        res.write('F', common.mustCall(() => callbacks.push('F')));

        const originalSend = res._send;
        res._send = common.mustCall(function(...args) {
          assert.notStrictEqual(this.socket.writableCorked, 0);
          return originalSend.apply(this, args);
        }, 5);
        try {
          res.uncork();
          res.uncork();
        } finally {
          res._send = originalSend;
        }

        // end() must flush this buffer before the terminating chunk.
        res.cork();
        res.cork();
        res.write('G', common.mustCall(() => callbacks.push('G')));
        res.addTrailers({ 'x-test': 'yes' });
        res.once('finish', common.mustCall(() => callbacks.push('finish')));
        res.end('H', common.mustCall(() => {
          callbacks.push('end');
          assert.deepStrictEqual(callbacks, ['E', 'F', 'G', 'finish', 'end']);
        }));
        assert.strictEqual(res.writableCorked, 0);
      }));
    }

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const callbacks = [];
      const req = transport.request({
        host: common.localhostIPv4,
        port: server.address().port,
        method: 'POST',
        ...requestOptions,
      }, common.mustCall((res) => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (chunk) => body += chunk);
        res.on('end', common.mustCall(() => {
          assert.strictEqual(body, 'EFGH');
          assert.strictEqual(res.trailers['x-test'], 'yes');
          server.close(common.mustCall(resolve));
        }));
      }));

      req.on('error', reject);
      req.cork();
      req.cork();
      req.write('A', common.mustCall(() => callbacks.push('A')));
      req.write('B', common.mustCall(() => callbacks.push('B')));
      req.uncork();
      req.uncork();
      req.cork();
      req.cork();
      req.write('C', common.mustCall(() => callbacks.push('C')));
      req.once('finish', common.mustCall(() => callbacks.push('finish')));
      req.end('D', common.mustCall(() => {
        callbacks.push('end');
        assert.deepStrictEqual(callbacks, ['A', 'B', 'C', 'finish', 'end']);
      }));
      assert.strictEqual(req.writableCorked, 0);
    }));
  });
}

function runPipelined() {
  return new Promise((resolve, reject) => {
    let firstResponse;
    const server = http.createServer(common.mustCall((req, res) => {
      if (req.url === '/first') {
        firstResponse = res;
        return;
      }

      assert.strictEqual(req.url, '/second');
      assert.strictEqual(res.socket, null);
      res.cork();
      res.cork();
      res.write('B');
      res.write('C');
      res.end();
      assert.strictEqual(res.writableCorked, 0);
      firstResponse.end('A');
    }, 2));

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
        assert.match(response, /\r\n\r\nAHTTP\/1\.1 200 OK\r\n/);
        assert.match(response, /\r\n\r\n1\r\nB\r\n1\r\nC\r\n0\r\n\r\n$/);
        server.close(common.mustCall(resolve));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.end(
          'GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n' +
          'GET /second HTTP/1.1\r\nHost: localhost\r\n' +
          'Connection: close\r\n\r\n',
        );
      }));
    }));
  });
}

function runDrainOnEnd() {
  return new Promise((resolve, reject) => {
    const server = http.createServer(common.mustCall((req, res) => {
      res.cork();
      assert.strictEqual(res.write('1'.repeat(10)), true);
      assert.strictEqual(res.write('2'.repeat(1000)), false);
      assert.strictEqual(res.writableNeedDrain, true);

      res.once('drain', common.mustCall(() => {
        assert.strictEqual(res.finished, true);
        assert.strictEqual(res.writableNeedDrain, false);
        assert.strictEqual(res.writableLength, 0);
      }));
      res.end();
    }));

    server.on('connection', common.mustCall((socket) => {
      socket._writableState.highWaterMark = 1000;
    }));
    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const req = http.get({
        host: common.localhostIPv4,
        port: server.address().port,
      }, common.mustCall((res) => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (chunk) => body += chunk);
        res.on('end', common.mustCall(() => {
          assert.strictEqual(body, '1'.repeat(10) + '2'.repeat(1000));
          server.close(common.mustCall(resolve));
        }));
      }));
      req.on('error', reject);
    }));
  });
}

async function main() {
  await runRoundTrip(http);

  if (common.hasCrypto) {
    const fixtures = require('../common/fixtures');
    const https = require('https');
    await runRoundTrip(https, {
      key: fixtures.readKey('agent1-key.pem'),
      cert: fixtures.readKey('agent1-cert.pem'),
    }, { rejectUnauthorized: false });
  }

  await runPipelined();
  await runDrainOnEnd();
}

main().then(common.mustCall());
