import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('Loader hooks throwing errors', { concurrency: true }, () => {
  it('throws on nonexistent modules', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "nonexistent/file.mjs"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
  });

  it('throws on unknown extensions', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import '${fixtures.fileURL('/es-modules/file.unknown')}'`,
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
  });

  it('throws on invalid return values', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/badReturnVal.mjs"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_INVALID_RETURN_VALUE/);
  });

  it('throws on boolean false', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/format.false"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_INVALID_RETURN_PROPERTY_VALUE/);
  });
  it('throws on boolean true', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/format.true"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_INVALID_RETURN_PROPERTY_VALUE/);
  });

  it('throws on invalid returned object', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/badReturnFormatVal.mjs"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_INVALID_RETURN_PROPERTY_VALUE/);
  });

  it('throws on unsupported format', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/unsupportedReturnFormatVal.mjs"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_UNKNOWN_MODULE_FORMAT/);
  });

  it('throws on invalid format property type', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      'import "esmHook/badReturnSourceVal.mjs"',
    ]);

    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.match(stderr, /ERR_INVALID_RETURN_PROPERTY_VALUE/);
  });

  it('rejects dynamic imports for all of the error cases checked above', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await Promise.allSettled([
        import('nonexistent/file.mjs'),
        import('${fixtures.fileURL('/es-modules/file.unknown')}'),
        import('esmHook/badReturnVal.mjs'),
        import('esmHook/format.false'),
        import('esmHook/format.true'),
        import('esmHook/badReturnFormatVal.mjs'),
        import('esmHook/unsupportedReturnFormatVal.mjs'),
        import('esmHook/badReturnSourceVal.mjs'),
      ]).then((results) => {
        assert.strictEqual(results.every((result) => result.status === 'rejected'), true);
      })`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });
});

describe('Loader hooks parsing modules', { concurrency: true }, () => {
  it('can parse .js files as ESM', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await import('${fixtures.fileURL('/es-module-loaders/js-as-esm.js')}')
      .then((parsedModule) => {
        assert.strictEqual(typeof parsedModule, 'object');
        assert.strictEqual(parsedModule.namedExport, 'named-export');
      })`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });

  it('can define .ext files as ESM', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await import('${fixtures.fileURL('/es-modules/file.ext')}')
      .then((parsedModule) => {
        assert.strictEqual(typeof parsedModule, 'object');
        const { default: defaultExport } = parsedModule;
        assert.strictEqual(typeof defaultExport, 'function');
        assert.strictEqual(defaultExport.name, 'iAmReal');
        assert.strictEqual(defaultExport(), true);
      })`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });

  it('can predetermine the format in the custom loader resolve hook', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await import('esmHook/preknownFormat.pre')
      .then((parsedModule) => {
        assert.strictEqual(typeof parsedModule, 'object');
        assert.strictEqual(parsedModule.default, 'hello world');
      })`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });

  it('can provide source for a nonexistent file', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await import('esmHook/virtual.mjs')
      .then((parsedModule) => {
        assert.strictEqual(typeof parsedModule, 'object');
        assert.strictEqual(typeof parsedModule.default, 'undefined');
        assert.strictEqual(parsedModule.message, 'WOOHOO!');
      })`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });

  it('ensures that loaders have a separate context from userland', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-custom.mjs'),
      '--input-type=module',
      '--eval',
      `import assert from 'node:assert';
      await import('${fixtures.fileURL('/es-modules/stateful.mjs')}')
      .then(({ default: count }) => {
        assert.strictEqual(count(), 1);
      });`,
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });

  it('ensures that user loaders are not bound to the internal loader', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/loader-this-value-inside-hook-functions.mjs'),
      '--input-type=module',
      '--eval',
      ';', // Actual test is inside the loader module.
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
  });
});
