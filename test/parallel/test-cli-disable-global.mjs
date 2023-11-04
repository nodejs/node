import { spawnPromisified } from '../common/index.mjs';
import { describe, it } from 'node:test';
import { strictEqual } from 'node:assert';


describe('--disable-global', { concurrency: true }, () => {
  it('disables one global', async () => {
    const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
      '--disable-global', 'navigator',
      '--eval', 'console.log(typeof navigator)',
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'undefined\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('disables all the globals in the supported list', async () => {
    const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
      '--disable-global', 'CustomEvent',
      '--disable-global', 'fetch',
      '--disable-global', 'navigator',
      '--disable-global', 'WebSocket',
      '--print',
      `[
        'CustomEvent',
        'fetch', 'FormData', 'Headers', 'Request', 'Response',
        'navigator',
        'WebSocket',
      ].filter(name => typeof globalThis[name] !== 'undefined')`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '[]\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('is case insensitive', async () => {
    const { stdout, stderr, code, signal } = await spawnPromisified(process.execPath, [
      '--disable-global', 'customevent',
      '--disable-global', 'websocket',
      '--print',
      `[
        'CustomEvent',
        'WebSocket',
      ].filter(name => typeof globalThis[name] !== 'undefined')`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '[]\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
