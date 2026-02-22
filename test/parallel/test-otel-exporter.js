'use strict';
const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

// This test verifies exporter error handling and process lifecycle behavior.

describe('node:otel exporter behavior', () => {
  it('does not crash when collector is unreachable', async () => {
    const { spawnPromisified } = common;
    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      `
      const http = require('node:http');
      const otel = require('node:otel');

      otel.start({ endpoint: 'http://127.0.0.1:1' });

      const server = http.createServer((req, res) => {
        res.writeHead(200);
        res.end('ok');
      });

      server.listen(0, () => {
        http.get('http://127.0.0.1:' + server.address().port, (res) => {
          res.resume();
          res.on('end', () => {
            server.close();
            otel.stop();
            console.log('success');
          });
        });
      });
      `,
    ]);

    assert.strictEqual(code, 0);
    assert.match(stdout, /success/);
  });

  it('flush timer does not keep process alive', async () => {
    const { spawnPromisified } = common;
    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      `
      const otel = require('node:otel');
      otel.start({ endpoint: 'http://127.0.0.1:1' });
      // Don't call otel.stop() -- the process should still exit
      // because the flush timer is unref'd.
      console.log('exiting');
      `,
    ], {
      timeout: 10_000,
    });

    assert.strictEqual(code, 0);
    assert.match(stdout, /exiting/);
  });
});
