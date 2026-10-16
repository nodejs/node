'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { describe, it } = require('node:test');
const {
  startOTelCollector,
  getSpans,
} = require('../common/otel');

// Tracing is activated only via environment variables and only when the
// --experimental-otel flag is passed. These tests spawn child processes to
// exercise that activation path.

const activationScript = `
const { isActive, getEndpoint } = require('internal/otel/core');
console.log('active:' + isActive());
console.log('endpoint:' + getEndpoint());
`;

function spawnOtel(env, args = []) {
  return common.spawnPromisified(
    process.execPath,
    ['--expose-internals', ...args, '-e', activationScript],
    { env: { ...process.env, ...env } },
  );
}

describe('otel environment variable activation', () => {
  it('activates tracing when NODE_OTEL_ENDPOINT is set with the flag', async () => {
    const { code, stdout, stderr } = await spawnOtel({
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:true/);
    assert.match(stdout, /endpoint:http:\/\/127\.0\.0\.1:9999/);
    assert.match(stderr, /ExperimentalWarning/);
  });

  it('activates tracing with the default endpoint when NODE_OTEL is set with the flag', async () => {
    const { code, stdout } = await spawnOtel({
      NODE_OTEL: '1',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:true/);
    assert.match(stdout, /endpoint:http:\/\/localhost:4318/);
  });

  it('NODE_OTEL_ENDPOINT takes precedence over NODE_OTEL', async () => {
    const { code, stdout } = await spawnOtel({
      NODE_OTEL: '1',
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /endpoint:http:\/\/127\.0\.0\.1:9999/);
  });

  it('ignores NODE_OTEL values other than 1', async () => {
    const { code, stdout, stderr } = await spawnOtel({
      NODE_OTEL: '0',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:false/);
    assert.strictEqual(stderr, '');
  });

  it('does not activate tracing without the --experimental-otel flag', async () => {
    const { code, stdout, stderr } = await spawnOtel({
      NODE_OTEL: '1',
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
    });

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:false/);
    assert.match(stdout, /endpoint:null/);
    assert.match(stderr, /OTelWarning/);
    assert.match(stderr, /--experimental-otel/);
  });

  it('NODE_OTEL_FILTER accepts node:fetch as an alias for undici', async () => {
    const { code, stdout } = await common.spawnPromisified(process.execPath, [
      '--experimental-otel',
      '--expose-internals',
      '-e',
      `
      const { isModuleEnabled } = require('internal/otel/core');
      console.log('fetch:' + isModuleEnabled('node:fetch'));
      console.log('http:' + isModuleEnabled('node:http'));
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: 'http://127.0.0.1:1',
        NODE_OTEL_FILTER: 'node:fetch',
      },
    });

    assert.strictEqual(code, 0);
    const lines = stdout.trim().split('\n');
    assert.match(lines.find((l) => l.startsWith('fetch:')), /fetch:true/);
    assert.match(lines.find((l) => l.startsWith('http:')), /http:false/);
  });

  it('flushes when the buffer fills, without waiting for exit', async () => {
    const collector = await startOTelCollector();

    // The child traces its own requests. Each request produces a server
    // and a client span, so a buffer size of 10 flushes long before the
    // child exits (it never exits on its own; the parent kills it).
    const script = `
    const http = require('node:http');
    const server = http.createServer((req, res) => {
      res.writeHead(200);
      res.end('ok');
    });
    server.listen(0, '127.0.0.1', () => {
      const port = server.address().port;
      let remaining = 12;
      function next() {
        if (remaining-- === 0) return;
        http.get('http://127.0.0.1:' + port + '/fill', () => next()).on('error', () => {});
      }
      next();
    });
    `;

    const child = spawn(process.execPath, ['--experimental-otel', '-e', script], {
      stdio: 'pipe',
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: `http://127.0.0.1:${collector.port}`,
        NODE_OTEL_MAX_BUFFER_SIZE: '10',
      },
    });

    // The payload must arrive while the child is still alive; if the child
    // exits first, the flush only happened at exit and the test fails.
    const spans = await Promise.race([
      collector.next().then(getSpans),
      new Promise((_, reject) => {
        child.on('exit', () => reject(new Error('child exited before flush')));
      }),
    ]);

    child.kill();
    await collector.close();

    assert.ok(spans.length > 0, 'Expected spans flushed by the full buffer');
  });

  it('does not activate tracing in worker threads', async () => {
    const { code, stdout } = await common.spawnPromisified(process.execPath, [
      '--experimental-otel',
      '--expose-internals',
      '-e',
      `
      const { Worker } = require('node:worker_threads');
      const { isActive } = require('internal/otel/core');
      console.log('main:' + isActive());
      const worker = new Worker(
        "const { isActive } = require('internal/otel/core');" +
        "console.log('worker:' + isActive());",
        { eval: true });
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: 'http://127.0.0.1:1',
      },
    });

    assert.strictEqual(code, 0);
    const lines = stdout.trim().split('\n');
    assert.match(lines.find((l) => l.startsWith('main:')), /main:true/);
    assert.match(lines.find((l) => l.startsWith('worker:')), /worker:false/);
  });

  it('does not activate tracing when no environment variables are set', async () => {
    const { code, stdout, stderr } = await spawnOtel({}, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:false/);
    assert.strictEqual(stderr, '');
  });

  it('NODE_OTEL_FILTER limits instrumented modules', async () => {
    const { code, stdout } = await common.spawnPromisified(process.execPath, [
      '--experimental-otel',
      '--expose-internals',
      '-e',
      `
      const { isModuleEnabled } = require('internal/otel/core');
      console.log('http:' + isModuleEnabled('node:http'));
      console.log('net:' + isModuleEnabled('node:net'));
      console.log('dns:' + isModuleEnabled('node:dns'));
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: 'http://127.0.0.1:1',
        NODE_OTEL_FILTER: 'node:http,node:net',
      },
    });

    assert.strictEqual(code, 0);
    const lines = stdout.trim().split('\n');
    assert.match(lines.find((l) => l.startsWith('http:')), /http:true/);
    assert.match(lines.find((l) => l.startsWith('net:')), /net:true/);
    assert.match(lines.find((l) => l.startsWith('dns:')), /dns:false/);
  });

  it('accepts NODE_OTEL_MAX_BUFFER_SIZE and NODE_OTEL_FLUSH_INTERVAL', async () => {
    const { code, stdout } = await spawnOtel({
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
      NODE_OTEL_MAX_BUFFER_SIZE: '250',
      NODE_OTEL_FLUSH_INTERVAL: '2500',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:true/);
  });

  it('warns and does not activate on an invalid NODE_OTEL_MAX_BUFFER_SIZE', async () => {
    const { code, stdout, stderr } = await spawnOtel({
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
      NODE_OTEL_MAX_BUFFER_SIZE: 'abc',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:false/);
    assert.match(stderr, /OTelWarning/);
    assert.match(stderr, /Failed to initialize OpenTelemetry tracing/);
  });

  it('warns and does not activate on an invalid NODE_OTEL_FLUSH_INTERVAL', async () => {
    const { code, stdout, stderr } = await spawnOtel({
      NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
      NODE_OTEL_FLUSH_INTERVAL: '0',
    }, ['--experimental-otel']);

    assert.strictEqual(code, 0);
    assert.match(stdout, /active:false/);
    assert.match(stderr, /OTelWarning/);
    assert.match(stderr, /Failed to initialize OpenTelemetry tracing/);
  });

  it('emits a warning and continues when the endpoint is invalid', async () => {
    const { code, stdout, stderr } = await common.spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      `
      const http = require('node:http');
      const server = http.createServer((req, res) => {
        res.writeHead(200);
        res.end('ok');
      });
      server.listen(0, () => {
        http.get('http://127.0.0.1:' + server.address().port, (res) => {
          res.resume();
          res.on('end', () => {
            server.close();
            console.log('still running');
          });
        });
      });
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: 'not-a-valid-url',
      },
    });

    assert.strictEqual(code, 0);
    assert.match(stdout, /still running/);
    assert.match(stderr, /OTelWarning/);
    assert.match(stderr, /Failed to initialize OpenTelemetry tracing/);
  });
});
