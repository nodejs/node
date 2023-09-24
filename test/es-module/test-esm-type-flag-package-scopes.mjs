import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import { strictEqual } from 'node:assert';

describe('the type flag should change the interpretation of certain files within a "type": "module" package scope', { concurrency: true }, () => {
  it('should run as ESM an extensionless JavaScript file within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/noext-esm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import an extensionless JavaScript file within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/imports-noext.mjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as Wasm an extensionless Wasm file within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--experimental-wasm-modules',
      '--no-warnings',
      fixtures.path('es-modules/package-type-module/noext-wasm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as Wasm an extensionless Wasm file within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--experimental-wasm-modules',
      '--no-warnings',
      '--eval',
      `import { add } from ${JSON.stringify(fixtures.fileURL('es-modules/package-type-module/noext-wasm'))};
      console.log(add(1, 2));`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '3\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});

describe('the type flag should change the interpretation of certain files within a package scope that lacks a "type" field and is not under node_modules', { concurrency: true }, () => {
  it('should run as ESM a .js file within package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-without-type/module.js'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as ESM an extensionless JavaScript file within a package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-without-type/noext-esm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as Wasm an extensionless Wasm file within a package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--experimental-wasm-modules',
      '--no-warnings',
      fixtures.path('es-modules/noext-wasm'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as ESM a .js file within package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('es-modules/package-without-type/module.js'))};`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as ESM an extensionless JavaScript file within a package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('es-modules/package-without-type/noext-esm'))};`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as Wasm an extensionless Wasm file within a package scope that has no defined "type" and is not under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--experimental-wasm-modules',
      '--no-warnings',
      '--eval',
      `import { add } from ${JSON.stringify(fixtures.fileURL('es-modules/noext-wasm'))};
      console.log(add(1, 2));`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, '3\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});

describe('the type flag should NOT change the interpretation of certain files within a package scope that lacks a "type" field and is under node_modules', { concurrency: true }, () => {
  it('should run as CommonJS a .js file within package scope that has no defined "type" and is under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/node_modules/dep/run.js'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as CommonJS a .js file within a package scope that has no defined "type" and is under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('es-modules/package-type-module/node_modules/dep/run.js'))};`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should run as CommonJS an extensionless JavaScript file within a package scope that has no defined "type" and is under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/node_modules/dep/noext-cjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('should import as CommonJS an extensionless JavaScript file within a package scope that has no defined "type" and is under node_modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('es-modules/package-type-module/node_modules/dep/noext-cjs'))};`,
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout, 'executed\n');
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
