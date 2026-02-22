'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const { describe, it } = require('node:test');

describe('node:otel environment variable configuration', () => {
  it('auto-activates tracing when NODE_OTEL_ENDPOINT is set', async () => {
    const { spawnPromisified } = common;

    const collector = http.createServer((req, res) => {
      req.on('data', () => {});
      req.on('end', () => {
        res.writeHead(200);
        res.end();
      });
    });

    await new Promise((resolve) => collector.listen(0, resolve));
    const collectorPort = collector.address().port;

    try {
      const { code, stdout } = await spawnPromisified(process.execPath, [
        '-e',
        `
        const otel = require('node:otel');
        console.log(otel.active);
        `,
      ], {
        env: {
          ...process.env,
          NODE_OTEL_ENDPOINT: `http://127.0.0.1:${collectorPort}`,
        },
      });

      assert.strictEqual(code, 0);
      assert.match(stdout.trim(), /true/);
    } finally {
      collector.close();
    }
  });

  it('NODE_OTEL_FILTER limits instrumented modules', async () => {
    const { spawnPromisified } = common;
    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--expose-internals',
      '-e',
      `
      const otel = require('node:otel');
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

  it('activates tracing with default endpoint when NODE_OTEL=1', async () => {
    const { spawnPromisified } = common;

    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--expose-internals',
      '-e',
      `
      const otel = require('node:otel');
      const { getEndpoint } = require('internal/otel/core');
      console.log('active:' + otel.active);
      console.log('endpoint:' + getEndpoint());
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL: '1',
      },
    });

    assert.strictEqual(code, 0);
    const lines = stdout.trim().split('\n');
    assert.match(lines.find((l) => l.startsWith('active:')), /active:true/);
    assert.match(
      lines.find((l) => l.startsWith('endpoint:')),
      /endpoint:http:\/\/localhost:4318/,
    );
  });

  it('NODE_OTEL_ENDPOINT overrides the default', async () => {
    const { spawnPromisified } = common;

    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--expose-internals',
      '-e',
      `
      const otel = require('node:otel');
      const { getEndpoint } = require('internal/otel/core');
      console.log('endpoint:' + getEndpoint());
      `,
    ], {
      env: {
        ...process.env,
        NODE_OTEL: '1',
        NODE_OTEL_ENDPOINT: 'http://127.0.0.1:9999',
      },
    });

    assert.strictEqual(code, 0);
    assert.match(stdout.trim(), /endpoint:http:\/\/127\.0\.0\.1:9999/);
  });

  it('emits warning and continues when env var endpoint is invalid', async () => {
    const { spawnPromisified } = common;

    const { code, stderr } = await spawnPromisified(process.execPath, [
      '-e',
      'console.log("still running");',
    ], {
      env: {
        ...process.env,
        NODE_OTEL_ENDPOINT: 'not-a-valid-url',
      },
    });

    assert.strictEqual(code, 0);
    assert.match(stderr, /OTelWarning/);
    assert.match(stderr, /Failed to initialize OpenTelemetry tracing/);
  });
});
