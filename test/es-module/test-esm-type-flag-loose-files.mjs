import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import { strictEqual } from 'node:assert';

describe('the type flag should change the interpretation of certain files outside of any package scope', { concurrency: true }, () => {
  it('should run as ESM a .js file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/loose.js'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as ESM an extensionless JavaScript file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/noext-esm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as Wasm an extensionless Wasm file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/noext-wasm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as ESM a .js file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/imports-loose.mjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as ESM an extensionless JavaScript file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/imports-noext.mjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as Wasm an extensionless Wasm file that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--eval',
      `import { add } from ${JSON.stringify(fixtures.fileURL('es-modules/noext-wasm'))};
      console.log(add(1, 2));`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '3\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should check as ESM input passed via --check', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--check',
      fixtures.path('es-modules/loose.js'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
