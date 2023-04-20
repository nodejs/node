import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('Worker threads do not spawn infinitely', { concurrency: true }, () => {
  it('should not trigger an infinite loop when using a loader exports no recognized hooks', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('empty.js'),
      '--eval',
      'setTimeout(() => console.log("hello"),99)',
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^hello\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should support a CommonJS entry point and a loader that imports a CommonJS module', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/loader-with-dep.mjs'),
      fixtures.path('print-delayed.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^delayed\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should support --require and --import along with using a loader written in CJS and CJS entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--eval',
      'setTimeout(() => console.log("D"),99)',
      '--import',
      fixtures.fileURL('printC.js'),
      '--experimental-loader',
      fixtures.fileURL('printB.js'),
      '--require',
      fixtures.path('printA.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^A\r?\nA\r?\nB\r?\nC\r?\nD\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should support --require and --import along with using a loader written in ESM and ESM entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--require',
      fixtures.path('printA.js'),
      '--experimental-loader',
      'data:text/javascript,import{writeFileSync}from"node:fs";writeFileSync(1, "B\n")',
      '--import',
      fixtures.fileURL('printC.js'),
      '--input-type=module',
      '--eval',
      'setTimeout(() => console.log("D"),99)',
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^A\r?\nA\r?\nB\r?\nC\r?\nD\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
