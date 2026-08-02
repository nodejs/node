// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const {
  kInternalWritev,
  kRawWritev,
} = require('internal/streams/utils');

function runMixedSocketWrites() {
  return new Promise((resolve, reject) => {
    const vector = [];
    const vectorBody = [];
    const encodedStrings = [
      ['\u0100', 'utf8'],
      ['\u0100', 'utf-8'],
      ['\u0100', 'utf16le'],
      ['\u0100', 'utf-16le'],
      ['\u0100', 'ucs2'],
      ['\u0100', 'ucs-2'],
      ['\u0100', 'ascii'],
      ['\u0100', 'latin1'],
      ['\u0100', 'binary'],
      ['QQ==', 'base64'],
      ['QQ', 'base64url'],
      ['41', 'hex'],
    ];
    for (let i = 0; i < 32; i++) {
      const [chunk, encoding] = encodedStrings[i % encodedStrings.length];
      vector.push(chunk, encoding);
      vectorBody.push(Buffer.from(chunk, encoding));
    }
    const expected = Buffer.concat([
      Buffer.from('A'),
      ...vectorBody,
      Buffer.from('DE'),
    ]);

    const server = net.createServer(common.mustCall((socket) => {
      const body = [];
      socket.on('data', (chunk) => body.push(chunk));
      socket.on('end', common.mustCall(() => {
        assert.deepStrictEqual(Buffer.concat(body), expected);
        server.close(common.mustCall(resolve));
      }));
    }));

    server.on('error', reject);
    server.listen(0, common.localhostIPv4, common.mustCall(() => {
      const socket = net.createConnection({
        host: common.localhostIPv4,
        port: server.address().port,
      });
      const callbacks = [];
      socket.on('error', reject);
      socket.on('connect', common.mustCall(() => {
        const originalWritev = socket[kRawWritev];
        socket[kRawWritev] = common.mustCall(function(chunks, callback) {
          assert.strictEqual(chunks.length, 68);
          for (let i = 1; i <= 32; i++) {
            assert.strictEqual(typeof chunks[i * 2 + 1], 'number');
          }
          return originalWritev.call(this, chunks, callback);
        });

        socket.cork();
        socket.write('A', common.mustCall(() => callbacks.push('A')));
        socket[kInternalWritev](vector,
                                common.mustCall(() => callbacks.push('vector')));
        socket.write(new Uint8Array([0x44]),
                     common.mustCall(() => callbacks.push('D')));
        assert.strictEqual(socket.bytesWritten, expected.length - 1);
        socket.uncork();
        socket.end('E', common.mustCall(() => {
          callbacks.push('E');
          assert.deepStrictEqual(callbacks, ['A', 'vector', 'D', 'E']);
        }));
      }));
    }));
  });
}

runMixedSocketWrites().then(common.mustCall());
