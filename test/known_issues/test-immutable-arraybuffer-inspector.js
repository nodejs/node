// Flags: --js-immutable-arraybuffer --experimental-network-inspection
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfInspectorDisabled();

const assert = require('node:assert');

// Network inspection copies request body chunks, which must also work when
// the body is immutable. Isolate uncaught diagnostic-channel errors so both
// request paths can be tested.
if (process.argv[2]) {
  const { once } = require('node:events');
  const { Session } = require('node:inspector/promises');
  const kind = process.argv[2];
  const enabled = process.argv[3] === 'on';

  async function run() {
    const session = new Session();
    session.connect();
    const http = require(kind === 'http2' ? 'node:http2' : 'node:http');
    const expected = kind === 'http2' ? 'ABCDABCD' : 'ABCD';
    const server = http.createServer(common.mustCall((req, res) => {
      let body = '';
      req.setEncoding('utf8');
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', common.mustCall(() => {
        assert.strictEqual(body, expected);
        res.end(body);
      }));
    }));
    let client;
    try {
      if (enabled) {
        session.on('Network.requestWillBeSent', common.mustCall());
        await session.post('Network.enable');
      }
      server.listen(0, '127.0.0.1');
      await once(server, 'listening');
      const url = `http://127.0.0.1:${server.address().port}`;
      const chunk = Buffer.from(Uint8Array.from([65, 66, 67, 68]).buffer.transferToImmutable());
      let body = '';
      if (kind === 'http2') {
        client = http.connect(url);
        const req = client.request({ ':method': 'POST' });
        req.cork();
        req.write(chunk);
        req.end(chunk);
        for await (const data of req) body += data;
      } else {
        const stream = new ReadableStream({
          start(controller) {
            controller.enqueue(chunk);
            controller.close();
          },
        });
        const response = await fetch(url, {
          method: 'POST', body: stream, duplex: 'half',
        });
        body = await response.text();
      }
      assert.strictEqual(body, expected);
      assert.strictEqual(chunk.toString(), 'ABCD');
      assert.strictEqual(chunk.buffer.immutable, true);
    } finally {
      client?.destroy();
      server.closeAllConnections?.();
      if (server.listening) await new Promise((resolve) => server.close(resolve));
      session.disconnect();
    }
  }

  run().then(common.mustCall());
} else {
  const { test } = require('node:test');
  const { spawnSyncAndAssert } = require('../common/child_process');

  for (const kind of ['http2', 'fetch']) {
    test(`${kind} sends immutable chunks with network inspection`, () => {
      // The same request succeeds with inspection disabled.
      for (const inspection of ['off', 'on']) {
        spawnSyncAndAssert(process.execPath, [
          '--js-immutable-arraybuffer', '--experimental-network-inspection',
          __filename, kind, inspection,
        ], { timeout: common.platformTimeout(10000) }, { status: 0 });
      }
    });
  }
}
