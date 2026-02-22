'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');
const { flush } = require('internal/otel/flush');

describe('node:otel self-trace prevention', () => {
  it('does not create spans for export requests to the collector', async () => {
    const allSpans = [];
    let batchCount = 0;
    let resolveFirstBatch;
    const firstBatch = new Promise((r) => { resolveFirstBatch = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        const data = JSON.parse(body);
        const spans = data.resourceSpans[0].scopeSpans[0].spans;
        for (const s of spans) allSpans.push(s);
        res.writeHead(200);
        res.end();
        batchCount++;
        if (batchCount === 1) resolveFirstBatch();
      });
    });

    await new Promise((r) => collector.listen(0, r));
    const collectorPort = collector.address().port;

    otel.start({ endpoint: `http://127.0.0.1:${collectorPort}` });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}/test`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    // Flush while instrumentation is still active. This sends an HTTP
    // request to the collector, which fires http.client.request.created.
    // The self-trace check (getCollectorHost()) should prevent creating
    // a span for this export request.
    flush();

    await firstBatch;

    // Allow time for any inadvertent export-request spans to be buffered.
    await new Promise((r) => setTimeout(r, 100));

    // Flush again: if a span was created for the export request above,
    // it would now be in the buffer and sent to the collector.
    flush();
    await new Promise((r) => setTimeout(r, 100));

    otel.stop();

    collector.close();
    server.close();

    for (const span of allSpans) {
      if (span.attributes) {
        const urlAttr = span.attributes.find((a) => a.key === 'url.full');
        if (urlAttr) {
          assert.ok(
            !urlAttr.value.stringValue.includes(`:${collectorPort}`),
            `Span should not target collector: ${urlAttr.value.stringValue}`,
          );
        }
      }
    }

    // Should have exactly the user request spans (server + client),
    // not any spans for the export requests.
    const clientSpans = allSpans.filter((s) => s.kind === 3); // CLIENT
    for (const cs of clientSpans) {
      const urlAttr = cs.attributes?.find((a) => a.key === 'url.full');
      assert.ok(urlAttr, 'Client span should have url.full');
      assert.ok(
        urlAttr.value.stringValue.includes('/test'),
        `Client span should be for /test, got: ${urlAttr.value.stringValue}`,
      );
    }
  });
});
