'use strict';
// Flags: --experimental-otel --expose-internals

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

const otel = require('node:otel');
const { flush } = require('internal/otel/flush');

describe('node:otel filter', () => {
  it('does not create spans for filtered-out modules', async () => {
    let collectorHit = false;
    const collector = http.createServer((req, res) => {
      collectorHit = true;
      req.resume();
      req.on('end', () => {
        res.writeHead(200);
        res.end();
      });
    });

    await new Promise((r) => collector.listen(0, r));

    // Filter to a module that won't be exercised.
    otel.start({
      endpoint: `http://127.0.0.1:${collector.address().port}`,
      filter: ['node:dns'],
    });

    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });

    await new Promise((r) => server.listen(0, r));

    await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${server.address().port}`, (res) => {
        res.resume();
        res.on('end', resolve);
      }).on('error', reject);
    });

    // Flush explicitly — if any HTTP spans were created despite the filter,
    // they would be sent to the collector.
    flush();

    // Wait for any potential request to the collector.
    await new Promise((r) => setTimeout(r, 100));

    assert.strictEqual(collectorHit, false);

    otel.stop();
    server.close();
    collector.close();
  });
});
