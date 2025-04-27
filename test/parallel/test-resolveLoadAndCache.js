'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { resolve, load, resolveAndLoad } = require('node:module');
const { describe, it } = require('node:test');


describe('load', () => {
  it('should return null source for CJS module and built-in modules', async () => {
    assert.deepStrictEqual(await load(fixtures.fileURL('empty.js')), {
      __proto__: null,
      format: 'commonjs',
      source: null,
    });
    assert.deepStrictEqual(await load('node:fs'), {
      __proto__: null,
      format: 'builtin',
      source: null,
    });
  });

  it('should return full source for ESM module', async () => {
    assert.deepStrictEqual(await load(fixtures.fileURL('es-modules/print-3.mjs')), {
      __proto__: null,
      format: 'module',
      source: Buffer.from('console.log(3);\n'),
    });
  });
});

describe('resolveAndLoad', () => {
  it('should accept relative path when a base URL is provided', async () => {
    assert.deepStrictEqual(await resolveAndLoad('./print-3.mjs', fixtures.fileURL('es-modules/loop.mjs')), {
      __proto__: null,
      url: `${fixtures.fileURL('es-modules/print-3.mjs')}`,
      format: 'module',
      source: Buffer.from('console.log(3);\n'),
    });
  });

  it('should throw when parentURL is invalid', async () => {
    for (const invalid of [null, {}, [], () => {}, true, false, 1, 0]) {
      await assert.rejects(
        () => resolveAndLoad('', invalid),
        { code: 'ERR_INVALID_URL' },
      );
    }

    await assert.rejects(resolveAndLoad('', Symbol()), {
      name: 'TypeError',
    });
  });

  it('should accept a file URL (string), like from `import.meta.resolve()`', async () => {
    const url = `${fixtures.fileURL('es-modules/print-3.mjs')}`;
    assert.deepStrictEqual(await resolveAndLoad(url, 'data:'), {
      __proto__: null,
      url,
      format: 'module',
      source: Buffer.from('console.log(3);\n'),
    });
    assert.deepStrictEqual(await resolveAndLoad('./print-3.mjs', fixtures.fileURL('es-modules/loop.mjs').href), {
      __proto__: null,
      url,
      format: 'module',
      source: Buffer.from('console.log(3);\n'),
    });
  });

  it('should require import attribute to load JSON files', async () => {
    const url = 'data:application/json,{}';
    await assert.rejects(load(url), { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' });
    assert.deepStrictEqual(await load(url, { type: 'json' }), {
      __proto__: null,
      format: 'json',
      source: Buffer.from('{}'),
    });
  });

  it('should use the existing cache', async () => {
    const url = fixtures.fileURL('es-modules/print-3.mjs');
    assert.deepStrictEqual(await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--loader',
      fixtures.fileURL('es-module-loaders/loader-resolve-passthru.mjs'),
      '--loader',
      fixtures.fileURL('es-module-loaders/loader-load-passthru.mjs'),
      '-p',
      `import(${JSON.stringify(url)}).then(() => module.resolveAndLoad(${JSON.stringify(url)}, url.pathToFileURL(__filename)))`,
    ]), {
      stderr: '',
      stdout: [
        'resolve passthru',
        'load passthru',
        '3',
        'resolve passthru',
        'load passthru',
        'Promise {',
        '  [Object: null prototype] {',
        '    format: \'module\',',
        '    source: Uint8Array(16) [',
        '       99, 111, 110, 115, 111,',
        '      108, 101,  46, 108, 111,',
        '      103,  40,  51,  41,  59,',
        '       10',
        '    ],',
        `    url: '${url}'`,
        '  }',
        '}',
        '',
      ].join('\n'),
      code: 0,
      signal: null,
    });
  });
});
