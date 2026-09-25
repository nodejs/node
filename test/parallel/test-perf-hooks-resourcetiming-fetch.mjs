// The ESM common wrapper does not export localhostIPv4.
// eslint-disable-next-line node-core/require-common-first
import common from '../common/index.js';

import assert from 'node:assert';
import { once } from 'node:events';
import { createServer } from 'node:http';
import {
  PerformanceResourceTiming,
  performance,
} from 'node:perf_hooks';

// This test ensures that built-in fetch creates a PerformanceResourceTiming
// entry with the resource timing attributes exposed by Node.js.

const responseBody = 'Hello world';
const server = createServer(common.mustCall((req, res) => {
  res.end(responseBody);
}));

server.listen(0, common.localhostIPv4);
await once(server, 'listening');

async function closeServer() {
  if (server.listening) {
    await new Promise((resolve) => server.close(resolve));
  }
}

try {
  performance.clearResourceTimings();

  const url = `http://${common.localhostIPv4}:${server.address().port}/`;
  const response = await fetch(url);
  assert.strictEqual(await response.text(), responseBody);
  await closeServer();

  const entries = performance.getEntriesByName(url, 'resource');
  assert.strictEqual(entries.length, 1);

  const [entry] = entries;
  assert(entry instanceof PerformanceResourceTiming);
  assert.strictEqual(entry.initiatorType, 'fetch');
  assert.strictEqual(entry.responseStatus, 200);
  assert(entry.finalResponseHeadersStart > 0);
  assert.strictEqual(entry.firstInterimResponseStart, 0);
  assert.strictEqual(entry.responseStart, entry.finalResponseHeadersStart);
  assert(entry.responseEnd >= entry.responseStart);
  assert.strictEqual(entry.renderBlockingStatus, 'non-blocking');
  assert.strictEqual(entry.contentType, '');
  assert.strictEqual(entry.contentEncoding, '');

  const json = entry.toJSON();
  for (const name of [
    'finalResponseHeadersStart',
    'firstInterimResponseStart',
    'responseStart',
    'renderBlockingStatus',
    'contentType',
    'contentEncoding',
  ]) {
    assert.strictEqual(json[name], entry[name], name);
  }
} finally {
  performance.clearResourceTimings();
  await closeServer();
}
