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
  httpGet,
  delay,
} = require('../common/otel');

describe('otel self-trace prevention', () => {
  it('does not create spans for export requests to the collector', async () => {
    const collector = await startOTelCollector();

    otel.start({ endpoint: `http://127.0.0.1:${collector.port}` });

    const server = await startHTTPServer();

    await httpGet(server.port, '/test');

    // Flush while instrumentation is still active. This sends an HTTP
    // request to the collector, which fires http.client.request.created.
    // The self-trace check (getCollectorHost()) should prevent creating
    // a client span for this export request.
    flush();
    await collector.next();

    // Allow time for any inadvertent export-request spans to be buffered,
    // then flush again so that they would be exported and observable.
    await delay(100);
    flush();
    await delay(100);

    await collector.close();
    await server.close();

    const allSpans = collector.payloads.flatMap(getSpans);

    // The export requests themselves must not produce server spans: a
    // collector hosted in this process must not be traced by its own
    // export traffic, or it would feed itself indefinitely.
    for (const span of allSpans) {
      const pathAttr = span.attributes?.find((a) => a.key === 'url.path');
      if (pathAttr) {
        assert.notStrictEqual(pathAttr.value.stringValue, '/v1/traces');
      }
    }

    // No span should target the collector.
    for (const span of allSpans) {
      const urlAttr = span.attributes?.find((a) => a.key === 'url.full');
      if (urlAttr) {
        assert.ok(
          !urlAttr.value.stringValue.includes(`:${collector.port}`),
          `Span should not target collector: ${urlAttr.value.stringValue}`,
        );
      }
    }

    // Should have exactly the user request spans (server + client),
    // not any client spans for the export requests.
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
