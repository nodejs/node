// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { kInternalWritev } = require('internal/streams/utils');

function runPipelinedOutputVector() {
  return new Promise((resolve, reject) => {
    let firstResponse;
    let requests = 0;
    const callbacks = [];
    const vectorLengths = [];
    const server = http.createServer(common.mustCall((req, res) => {
      requests++;
      if (requests === 1) {
        firstResponse = res;
        return;
      }

      assert.strictEqual(res.socket, null);
      const socket = req.socket;
      const originalWritev = socket[kInternalWritev];
      socket[kInternalWritev] = common.mustCall(function(vector, callback) {
        vectorLengths.push(vector.length >> 1);
        return originalWritev.call(this, vector, callback);
      }, 2);

      res.write('A', common.mustCall(() => callbacks.push('A')));
      res.write(Buffer.from('B'), common.mustCall(() => callbacks.push('B')));
      res.end('C', common.mustCall(() => callbacks.push('end')));
      assert(res.outputData.length > 1);
      firstResponse.end('first');
    }, 2));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      socket.on('error', reject);
      socket.on('data', () => {});
      socket.on('end', common.mustCall(() => {
        assert.deepStrictEqual(callbacks, ['A', 'B', 'end']);
        assert.strictEqual(vectorLengths.length, 2);
        assert(vectorLengths[1] > 1);
        server.close(common.mustCall(resolve));
      }));
      socket.on('connect', common.mustCall(() => {
        socket.write(
          'GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n' +
          'GET /second HTTP/1.1\r\nHost: localhost\r\n' +
          'Connection: close\r\n\r\n',
        );
      }));
    }));
  });
}

runPipelinedOutputVector().then(common.mustCall());
