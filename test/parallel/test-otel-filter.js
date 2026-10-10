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
  httpGet,
  delay,
} = require('../common/otel');

describe('otel filter', () => {
  it('does not create spans for filtered-out modules', async () => {
    const collector = await startOTelCollector();

    // Filter to a module that will not be exercised.
    otel.start({
      endpoint: `http://127.0.0.1:${collector.port}`,
      filter: 'node:dns',
    });

    const server = await startHTTPServer();

    await httpGet(server.port, '/');

    // Flush explicitly — if any HTTP spans were created despite the filter,
    // they would be sent to the collector.
    flush();

    // Wait for any potential request to the collector.
    await delay(100);

    assert.strictEqual(collector.requests.length, 0);

    await collector.close();
    await server.close();
  });
});
