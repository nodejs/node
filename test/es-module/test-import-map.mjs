// Flags: --expose-internals

import { spawnPromisified } from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import tmpdir from '../common/tmpdir.js';
import { describe, it } from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { pathToFileURL } from 'node:url';
import { writeFile } from 'node:fs/promises';
import http from 'node:http';
import import_map from 'internal/modules/esm/import_map';
const { ImportMap } = import_map;
import binding from 'internal/test/binding';
const { primordials: { SafeMap, JSONStringify, ObjectEntries } } = binding;

const importMapFixture = (...p) => fixtures.path('es-module-loaders', 'import-maps', ...p);
const getImportMapPathURL = (name, filename = 'importmap.json') => {
  return pathToFileURL(importMapFixture(name, filename));
};
const readImportMapFile = async (name, filename) => {
  const url = getImportMapPathURL(name, filename);
  return [url, (await import(url, { with: { type: 'json' } })).default];
};
const getImportMap = async (name, filename) => {
  const [url, rawMap] = await readImportMapFile(name, filename);
  return new ImportMap(rawMap, url);
};
const spawnPromisifiedWtihImportMap = async (name) => {
  const entryPoint = importMapFixture(name, 'index.mjs');
  const importMapPath = getImportMapPathURL(name).pathname;
  const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
    '--no-warnings',
    '--experimental-import-map', importMapPath,
    entryPoint,
  ], {
    cwd: importMapFixture(name),
  });
  assert.strictEqual(code, 0, [stderr, stdout]);
  assert.strictEqual(signal, null);
  return stdout;
}

describe('Import Maps', { concurrency: true }, () => {
  it('processImportMap - simple import map', async () => {
    const importMap = await getImportMap('simple');
    assert.deepStrictEqual(importMap.imports, new SafeMap(Object.entries({
      foo: './foo/index.mjs'
    })));
    const expectedScopes = new SafeMap();
    const fooScopeKey = new URL(importMap.baseURL, pathToFileURL('node_modules/foo'));
    const fooScopeMap = new SafeMap(Object.entries({
      bar: './baz.mjs'
    }));
    expectedScopes.set(fooScopeKey, fooScopeMap);
    assert.deepStrictEqual(importMap.scopes, expectedScopes);
  });

  it('processImportMap - invalid import map', async () => {
    await assert.rejects(
      getImportMap('invalid'),
      {
        code: 'ERR_INVALID_IMPORT_MAP'
      }
    );
    await assert.rejects(
      getImportMap('invalid', 'missing-scopes.json'),
      {
        code: 'ERR_INVALID_IMPORT_MAP'
      }
    );
    await assert.rejects(
      getImportMap('invalid', 'array-imports.json'),
      {
        code: 'ERR_INVALID_IMPORT_MAP'
      }
    );
  });

  it('resolve - empty import map', async () => {
    const importMap = await getImportMap('empty');
    const spec = importMap.resolve('test');
    assert.strictEqual(spec, 'test');
  });

  it('resolve - simple import map', async () => {
    const importMap = await getImportMap('simple');
    const entryPoint = new URL('index.mjs', importMap.baseURL);
    assert.strictEqual(
      importMap.resolve('foo').pathname,
      new URL('foo/index.mjs', entryPoint).pathname
    );
    assert.strictEqual(
      importMap.resolve('bar', new URL('foo/index.mjs', entryPoint)).pathname,
      new URL('baz.mjs', entryPoint).pathname
    );
    assert.strictEqual(importMap.resolve('bar'), 'bar');
  });

  it('resolve - scope with correct precedence', async () => {
    const importMap = await getImportMap('scope-order');
    const entryPoint = new URL('index.mjs', importMap.baseURL);
    assert.strictEqual(
      importMap.resolve('zed', new URL('node_modules/bar', entryPoint)).pathname,
      new URL('node_modules/bar/node_modules/zed/index.mjs', entryPoint).pathname
    );
    assert.strictEqual(
      importMap.resolve('zed', new URL('node_modules/bar/node_modules/zed', entryPoint)).pathname,
      new URL('baz.mjs', entryPoint).pathname
    );
  });

  it('resolve - data url', async () => {
    const importMap = await getImportMap('data-uri');
    assert.strictEqual(
      importMap.resolve('foo').href,
      'data:text/javascript,export default () => \'data foo\''
    );
    assert.strictEqual(
      importMap.resolve('foo/bar').href,
      'data:text/javascript,export default () => \'data bar\''
    );
    assert.strictEqual(
      importMap.resolve('baz', new URL('./index.mjs', importMap.baseURL)).href,
      'data:text/javascript,export default () => \'data baz\''
    );
    assert.strictEqual(
      importMap.resolve('data:text/javascript,export default () => \'bad\'', new URL('./index.mjs', importMap.baseURL)).href,
      'data:text/javascript,export default () => \'data qux\''
    );
  });

  it('should throw on startup on invalid import map', async () => {
    await assert.rejects(
      spawnPromisifiedWtihImportMap('invalid'),
      /Error \[ERR_INVALID_IMPORT_MAP\]: Invalid import map: top level key "imports" is required and must be a plain object/
    );
  });

  it('should pass --experimental-import-map', async () => {
    const stdout = await spawnPromisifiedWtihImportMap('simple');
    assert.strictEqual(stdout, 'baz\n');
  });

  it('should handle import maps with data uris', async () => {
    const stdout = await spawnPromisifiedWtihImportMap('data-uri');
    assert.strictEqual(stdout, 'data foo\ndata bar\ndata baz\ndata qux\n');
  });

  it('should handle import maps with bare specifiers', async () => {
    const stdout = await spawnPromisifiedWtihImportMap('bare');
    assert.strictEqual(stdout, 'zed\n');
  });

  it('should handle import maps with absolute paths', async () => {
    tmpdir.refresh();
    const entryPoint = importMapFixture('simple', 'index.mjs');
    const importMapPath = path.resolve(tmpdir.path, 'absolute.json');

    // Read simple import map and convert to absolute file paths
    const [, simple] = await readImportMapFile('simple');
    const importsEntries = ObjectEntries(simple.imports);
    for (const { 0: key, 1: val } of importsEntries) {
      simple.imports[key] = importMapFixture('simple', val);
    }
    const scopesEntries = ObjectEntries(simple.scopes);
    for (const { 0: scope, 1: map } of scopesEntries) {
      const mapEntries = ObjectEntries(map);
      const absScope = importMapFixture('simple', scope);
      simple.scopes[absScope] = {};
      for (const { 0: key, 1: val } of mapEntries) {
        simple.scopes[absScope][key] = importMapFixture('simple', val);
      }
      delete simple.scopes[scope];
    }

    await writeFile(importMapPath, JSONStringify(simple));
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-import-map', importMapPath,
      entryPoint,
    ], {
      cwd: importMapFixture('simple'),
    });

    assert.strictEqual(code, 0, [stderr, stdout]);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'baz\n');
  });
  
  it('should handle import maps as data uris', async () => {
    const entryPoint = importMapFixture('data-uri', 'index.mjs');
    const [, importMap] = await readImportMapFile('data-uri');
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      // '--inspect', '--inspect-brk',
      '--experimental-import-map', `data:application/json,${JSONStringify(importMap)}`,
      entryPoint,
    ], {
      cwd: importMapFixture('data-uri'),
    });

    assert.strictEqual(code, 0, [stderr, stdout]);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'data foo\ndata bar\ndata baz\ndata qux\n');
  });

  it('should handle http imports', async () => {
    const entryPoint = importMapFixture('simple', 'index.mjs');
    const server = http.createServer((req, res) => {
      res
        .writeHead(200, { 'Content-Type': 'application/javascript' })
        .end('export default () => \'http\'');
    });
    await (new Promise((resolve, reject) => {
      server.listen((err) => {
        if (err) return reject(err);
        resolve();
      });
    }));
    const { port } = server.address();

    tmpdir.refresh();
    const importMapPath = path.resolve(tmpdir.path, 'http.json');
    await writeFile(importMapPath, JSONStringify({
      imports: {
        foo: `http://localhost:${port}`
      },
      scopes: {}
    }));

    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--experimental-network-imports',
      '--experimental-import-map', importMapPath,
      entryPoint,
    ], {
      cwd: importMapFixture('simple'),
    });

    server.close();
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'http\n');
  });

  it('should work with other loaders', async () => {
    const entryPoint = importMapFixture('simple', 'index.mjs');
    const importMapPath = getImportMapPathURL('simple').pathname;
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-import-map', importMapPath,
      '--loader', fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      entryPoint,
    ], {
      cwd: importMapFixture(),
    });

    assert.strictEqual(code, 0, [stderr, stdout].join('\n'));
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'baz\n');
  });
});
