'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const otel = require('internal/otel/core');
const { flush } = require('internal/otel/flush');
const {
  startOTelCollector,
  startHTTPServer,
  getSpans,
  spanAttrs,
} = require('../common/otel');

describe('otel undici spans', () => {
  it('creates client spans for fetch requests and overrides existing ' +
     'trace context headers', async () => {
    const collector = await startOTelCollector();

    otel.start({
      endpoint: `http://127.0.0.1:${collector.port}`,
      filter: 'node:undici,node:fetch',
    });

    const receivedTraceparent = {};
    const target = await startHTTPServer((req, res) => {
      receivedTraceparent[req.url] = req.headers.traceparent;
      res.writeHead(200);
      res.end('ok');
    });

    try {
      const res = await fetch(`http://127.0.0.1:${target.port}/fetch-test`);
      await res.text();
    } catch {
      // Ignore fetch errors.
    }

    // A caller-supplied traceparent must be overridden, not duplicated:
    // undici's addHeader appends, so a duplicate header would arrive at
    // the server as a single comma-separated value.
    try {
      const res = await fetch(`http://127.0.0.1:${target.port}/override-test`, {
        headers: {
          traceparent: '00-00000000000000000000000000000000-' +
                       '0000000000000000-00',
        },
      });
      await res.text();
    } catch {
      // Ignore fetch errors.
    }

    flush();
    const spans = getSpans(await collector.next());

    await collector.close();
    await target.close();

    assert.ok(spans.length > 0, 'Expected at least one span from fetch');
    const clientSpan = spans.find((s) => {
      return s.kind === 3 && s.attributes?.some(
        (a) => a.key === 'url.full' &&
               a.value.stringValue.includes('/fetch-test'));
    });
    assert.ok(clientSpan, 'Expected a CLIENT span from fetch');

    const attrs = spanAttrs(clientSpan);
    assert.strictEqual(attrs['http.request.method'], 'GET');
    assert.ok(attrs['url.full'], 'Expected url.full attribute');

    const overridden = receivedTraceparent['/override-test'];
    assert.ok(overridden, 'Expected a traceparent on the override request');
    assert.match(overridden, /^00-[0-9a-f]{32}-[0-9a-f]{16}-0[01]$/);
    assert.ok(!overridden.includes(','),
              'traceparent must be overridden, not duplicated');
  });
});
