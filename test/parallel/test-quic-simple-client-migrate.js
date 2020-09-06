// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { key, cert, ca } = require('../common/quic');

const { once } = require('events');
const { createQuicSocket } = require('net');
const { pipeline } = require('stream');

const options = { key, cert, ca, alpn: 'zzz' };

let req;
const client = createQuicSocket({ client: options });
const client2 = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

(async function() {
  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustCall(async (stream) => {
      pipeline(stream, stream, common.mustSucceed());
      (await session.openStream({ halfOpen: true }))
        .end('Hello from the server');
    }));
  }));

  await server.listen();

  req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  req.on('close', () => {
    client2.close();
    server.close();
  });

  req.on('stream', common.mustCall((stream) => {
    let data = '';
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'Hello from the server');
    }));
    stream.on('close', common.mustCall());
  }));

  let data = '';
  const stream = await req.openStream();
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => data += chunk);
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'Hello from the client');
  }));
  stream.on('close', common.mustCall(() => {
    req.close();
  }));
  // Send some data on one connection...
  stream.write('Hello ');

  // Wait just a bit, then migrate to a different
  // QuicSocket and continue sending.
  setTimeout(common.mustCall(async () => {
    const s1 = req.socket;
    const a1 = req.socket.endpoints[0].address;

    await Promise.all([1, {}, 'test', false, null, undefined].map((i) => {
      return assert.rejects(req.setSocket(i), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));
    await Promise.all([1, {}, 'test', null].map((i) => {
      return assert.rejects(req.setSocket(req.socket, i), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));

    await req.setSocket(client2);

    // Verify that it is using a different network endpoint
    assert.notStrictEqual(s1, req.socket);
    assert.notDeepStrictEqual(a1, req.socket.endpoints[0].address);
    client.close();
    stream.end('from the client');
  }), common.platformTimeout(100));

  await Promise.all([
    once(server, 'close'),
    once(client2, 'close')
  ]);
})().then(common.mustCall());
