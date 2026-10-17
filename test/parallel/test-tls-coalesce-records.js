'use strict';
// TLS records that are already buffered are emitted as one chunk rather than
// one chunk per record. The end of the stream, clean or not, must still be
// reported after the data that precedes it.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const RECORD_SIZE = 512;
const RECORDS = 256;
const payload = Buffer.alloc(RECORD_SIZE * RECORDS);
for (let i = 0; i < RECORDS; i++)
  payload.fill(i, i * RECORD_SIZE, (i + 1) * RECORD_SIZE);

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};
const clientOptions = {
  ca: fixtures.readKey('ca1-cert.pem'),
  servername: 'agent1',
};

function sendPayload(socket) {
  assert(socket.setMaxSendFragment(RECORD_SIZE));
  socket.end(payload);
}

// Small records are coalesced, and 'end' follows all of the data.
{
  const server = tls.createServer(serverOptions, common.mustCall(sendPayload));
  server.listen(0, common.mustCall(() => {
    const chunks = [];
    const client = tls.connect({ ...clientOptions, port: server.address().port });
    client.on('data', (chunk) => chunks.push(chunk));
    client.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(chunks), payload);
      assert(chunks.length < RECORDS, `${chunks.length} chunks`);
      assert(chunks.every((chunk) => chunk.length <= 16 * 1024));
      server.close();
    }));
  }));
}

// An onread buffer smaller than a record still receives every byte.
{
  const server = tls.createServer(serverOptions, common.mustCall(sendPayload));
  server.listen(0, common.mustCall(() => {
    const chunks = [];
    const client = tls.connect({
      ...clientOptions,
      port: server.address().port,
      onread: {
        buffer: Buffer.alloc(100),
        callback(nread, buf) {
          chunks.push(Buffer.from(buf.subarray(0, nread)));
        },
      },
    });
    client.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(chunks), payload);
      server.close();
    }));
  }));
}

// A bad record that arrives together with data is reported after the data.
{
  // Small enough to arrive in one read and to fit into one batch.
  const data = payload.subarray(0, 8 * RECORD_SIZE);
  const server = tls.createServer(serverOptions, common.mustCall((socket) => {
    socket.once('data', common.mustCall(() => {
      assert(socket.setMaxSendFragment(RECORD_SIZE));
      socket.end(data);
    }));
  }));
  server.listen(0, common.mustCall(() => {
    // Passes the handshake through, then holds the response until the server
    // ends and forwards it in one write, with the last data record (the one
    // before close_notify) corrupted.
    let holding = false;
    const held = [];
    const proxy = net.createServer(common.mustCall((socket) => {
      const upstream = net.connect(server.address().port);
      socket.pipe(upstream);
      socket.on('error', () => {});
      upstream.on('data', (chunk) => {
        if (holding)
          held.push(chunk);
        else
          socket.write(chunk);
      });
      upstream.on('end', common.mustCall(() => {
        const response = Buffer.concat(held);
        let closeNotify = 0;
        for (let i = 0; i < response.length; i += 5 + response.readUInt16BE(i + 3))
          closeNotify = i;
        response[closeNotify - 1] ^= 1;
        socket.end(response);
      }));
    }));
    proxy.listen(0, common.mustCall(() => {
      const chunks = [];
      const client = tls.connect({
        ...clientOptions,
        port: proxy.address().port,
      }, common.mustCall(() => {
        holding = true;
        client.write('go');
      }));
      client.on('data', (chunk) => chunks.push(chunk));
      client.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code,
                           'ERR_SSL_DECRYPTION_FAILED_OR_BAD_RECORD_MAC');
        assert.deepStrictEqual(Buffer.concat(chunks),
                               data.subarray(0, -RECORD_SIZE));
        proxy.close();
        server.close();
      }));
    }));
  }));
}
