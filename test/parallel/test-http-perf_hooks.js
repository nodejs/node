'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const { PerformanceObserver } = require('perf_hooks');
const entries = [];
const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  entries.push(...items.getEntries());
}));

obs.observe({ type: 'http' });

const expected = 'Post Body For Test';
const makeRequest = common.mustCall((options) => {
  return new Promise((resolve, reject) => {
    http.request(options, common.mustCall((res) => {
      resolve();
    })).on('error', reject).end(options.data);
  });
}, 2);

const server = http.Server(common.mustCall((req, res) => {
  let result = '';

  req.setEncoding('utf8');
  req.on('data', function(chunk) {
    result += chunk;
  });

  req.on('end', common.mustCall(function() {
    assert.strictEqual(result, expected);
    res.writeHead(200);
    res.end('hello world\n');
  }));
}, 2));

let port;
server.listen(0, common.mustCall(async () => {
  port = server.address().port;
  await Promise.all([
    makeRequest({
      port,
      path: '/',
      method: 'POST',
      data: expected
    }),
    makeRequest({
      port,
      path: '/',
      method: 'POST',
      data: expected
    }),
  ]);
  server.close();
}));

process.on('exit', () => {
  let numberOfHttpClients = 0;
  let numberOfHttpRequests = 0;
  for (const entry of entries) {
    assert.strictEqual(entry.entryType, 'http');
    assert.strictEqual(typeof entry.startTime, 'number');
    assert.strictEqual(typeof entry.duration, 'number');
    if (entry.name === 'HttpClient') {
      numberOfHttpClients++;
      // The reported URL must include the port when it is non-default.
      // Refs: https://github.com/nodejs/node/issues/59625
      assert.strictEqual(entry.detail.req.url, `http://localhost:${port}/`);
    } else if (entry.name === 'HttpRequest') {
      numberOfHttpRequests++;
      assert.strictEqual(entry.detail.req.url, '/');
    }
    assert.strictEqual(entry.detail.req.method, 'POST');
    assert.strictEqual(typeof entry.detail.req.headers, 'object');
    assert.strictEqual(entry.detail.res.statusCode, 200);
    assert.strictEqual(entry.detail.res.statusMessage, 'OK');
    assert.strictEqual(typeof entry.detail.res.headers, 'object');
  }
  assert.strictEqual(numberOfHttpClients, 2);
  assert.strictEqual(numberOfHttpRequests, 2);
});
