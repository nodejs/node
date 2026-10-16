'use strict';

require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { spawn } = require('node:child_process');
const {
  startOTelCollector,
  getSpans,
} = require('../common/otel');

// Test that buffered spans are flushed via the beforeExit handler when the
// process exits while tracing is active. Tracing is activated in the child
// via environment variables.

describe('otel beforeExit flush', () => {
  it('flushes buffered spans on process beforeExit', async () => {
    const collector = await startOTelCollector();

    // Spawn a child with tracing activated via NODE_OTEL_ENDPOINT. The child
    // makes a request and exits without any explicit shutdown. The beforeExit
    // handler should flush the buffered spans.
    const script = `
const http = require("http");
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
    ], {
      stdio: 'pipe',
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: `http://127.0.0.1:${collector.port}`,
      },
    });

    const exitCode = await new Promise((resolve) => {
      child.on('exit', (code) => resolve(code));
    });
    assert.strictEqual(exitCode, 0);

    const spans = getSpans(await collector.next());

    await collector.close();

    assert.ok(spans.length > 0, 'Expected spans to be flushed on beforeExit');
  });
});
