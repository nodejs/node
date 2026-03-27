'use strict';
// Flags: --experimental-otel

require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');
const { spawn } = require('node:child_process');

// Test that buffered spans are flushed via the beforeExit handler
// when the process exits without calling otel.stop().

describe('node:otel beforeExit flush', () => {
  it('flushes buffered spans on process beforeExit', async () => {
    let resolveSpans;
    const spansReceived = new Promise((r) => { resolveSpans = r; });

    const collector = http.createServer((req, res) => {
      let body = '';
      req.on('data', (chunk) => { body += chunk; });
      req.on('end', () => {
        const data = JSON.parse(body);
        const spans = data.resourceSpans[0].scopeSpans[0].spans;
        res.writeHead(200);
        res.end();
        resolveSpans(spans);
      });
    });

    await new Promise((r) => collector.listen(0, r));
    const collectorPort = collector.address().port;

    // Spawn a child that starts otel, makes a request, and exits
    // WITHOUT calling otel.stop(). The beforeExit handler should flush.
    const script = `
const http = require("http");
const otel = require("node:otel");
otel.start({ endpoint: "http://127.0.0.1:${collectorPort}" });
const server = http.createServer((req, res) => {
  res.writeHead(200);
  res.end("ok");
});
server.listen(0, () => {
  const port = server.address().port;
  http.get("http://127.0.0.1:" + port + "/test", (res) => {
    res.resume();
    res.on("end", () => {
      server.close();
    });
  });
});
`;

    const child = spawn(process.execPath, [
      '--experimental-otel', '-e', script,
    ], { stdio: 'pipe' });

    const childExited = new Promise((resolve) => {
      child.on('exit', (code) => resolve(code));
    });

    const exitCode = await childExited;
    assert.strictEqual(exitCode, 0);

    // Wait for the collector to receive spans.
    const timeout = setTimeout(() => {
      collector.close();
      assert.fail('Timed out waiting for spans from beforeExit flush');
    }, 10_000);
    timeout.unref();

    const spans = await spansReceived;
    clearTimeout(timeout);

    collector.close();

    assert.ok(spans.length > 0, 'Expected spans to be flushed on beforeExit');
  });
});
