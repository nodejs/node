'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('node:assert');
const https = require('node:https');
const fixtures = require('../common/fixtures');
const { describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  getSpans,
  spanAttrs,
} = require('../common/otel');

describe('otel HTTPS server spans', () => {
  it('marks the url.scheme attribute as https for TLS requests', async () => {
    if (!common.hasCrypto) {
      common.skip('missing crypto');
      return;
    }

    const collector = await startOTelCollector();

    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });

    const server = https.createServer({
      key: fixtures.readKey('agent1-key.pem'),
      cert: fixtures.readKey('agent1-cert.pem'),
    }, (req, res) => {
      res.writeHead(200);
      res.end('ok');
    });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));

    await new Promise((resolve, reject) => {
      https.get({
        host: '127.0.0.1',
        port: server.address().port,
        path: '/secure',
        rejectUnauthorized: false,
        agent: false,
      }, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    flush();
    const spans = getSpans(await collector.next());

    await collector.close();
    await new Promise((resolve) => server.close(resolve));

    const serverSpan = spans.find((s) => s.kind === 2); // SPAN_KIND_SERVER
    assert.ok(serverSpan, 'Expected a server span');

    const attrs = spanAttrs(serverSpan);
    assert.strictEqual(attrs['url.scheme'], 'https');
    assert.strictEqual(attrs['url.path'], '/secure');
  });
});
