import { skip, spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { test } from 'node:test';

test('expect process.features.typescript to be false when --no-experimental-strip-types ', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--no-experimental-strip-types',
    fixtures.path('typescript/echo-process-features-typescript.cjs'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, 'false\n');
  assert.strictEqual(result.code, 0);
});

test('expect process.features.typescript to be \'transform\' when --experimental-transform-types', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-transform-types',
    fixtures.path('typescript/echo-process-features-typescript.cjs'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, process.config.variables.node_use_amaro ? 'transform\n' : 'false\n');
  assert.strictEqual(result.code, 0);
});


if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('execute a TypeScript file', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-typescript.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with imports', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-import-foo.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with imports', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--eval',
    `require(${JSON.stringify(fixtures.path('typescript/ts/test-import-fs.ts'))})`,
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with node_modules', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-typescript-node-modules.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('expect error when executing a TypeScript file with imports with no extensions', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-import-no-extension.ts'),
  ]);

  assert.match(result.stderr, /Error \[ERR_MODULE_NOT_FOUND\]:/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('expect error when executing a TypeScript file with enum', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-enums.ts'),
  ]);

  // This error should be thrown during transformation
  assert.match(result.stderr, /TypeScript enum is not supported in strip-only mode/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('expect error when executing a TypeScript file with experimental decorators', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-experimental-decorators.ts'),
  ]);
  // This error should be thrown at runtime
  assert.match(result.stderr, /Invalid or unexpected token/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('expect error when executing a TypeScript file with namespaces', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-namespaces.ts'),
  ]);
  // This error should be thrown during transformation
  assert.match(result.stderr, /TypeScript namespace declaration is not supported in strip-only mode/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with type definition', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-import-types.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with type definition but no type keyword', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-import-no-type-keyword.ts'),
  ]);

  assert.match(result.stderr, /does not provide an export named 'MyType'/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with CommonJS syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-commonjs-parsing.ts'),
  ]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with ES module syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-module-typescript.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('expect failure of a TypeScript file requiring ES module syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-require-module',
    fixtures.path('typescript/ts/test-require-module.ts'),
  ]);

  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('expect stack trace of a TypeScript file to be correct', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-whitespacing.ts'),
  ]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /test-whitespacing\.ts:5:7/);
  assert.strictEqual(result.code, 1);
});

test('execute CommonJS TypeScript file from node_modules with require-module', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-import-ts-node-modules.ts'),
  ]);

  assert.match(result.stderr, /ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with CommonJS syntax requiring .cts', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-require-cts.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with CommonJS syntax requiring .mts', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-require-mts.ts'),
  ]);

  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with CommonJS syntax requiring .mts using require-module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-require-module',
    fixtures.path('typescript/ts/test-require-mts.ts'),
  ]);

  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with CommonJS syntax requiring .cts using commonjs', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-require-cts.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript file with CommonJS syntax requiring .mts with require-module',
     async () => {
       const result = await spawnPromisified(process.execPath, [
         '--no-warnings',
         fixtures.path('typescript/ts/test-require-cts.ts'),
       ]);

       assert.strictEqual(result.stderr, '');
       assert.match(result.stdout, /Hello, TypeScript!/);
       assert.strictEqual(result.code, 0);
     });

test('execute a JavaScript file importing a cjs TypeScript file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/issue-54457.mjs'),
  ]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript test mocking module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--test',
    '--experimental-test-module-mocks',
    '--no-warnings',
    fixtures.path('typescript/ts/test-mock-module.ts'),
  ]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript-Module!/);
  assert.match(result.stdout, /Hello, TypeScript-CommonJS!/);
  assert.strictEqual(result.code, 0);
});

test('expect process.features.typescript to be strip', async () => {
  assert.strictEqual(process.features.typescript, 'strip');
});

test('execute a TypeScript file with union types', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-union-types.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout,
                     '{' +
                     " name: 'Hello, TypeScript!' }\n" +
                     '{ role: \'admin\', permission: \'all\' }\n' +
                     '{\n  foo: \'Testing Partial Type\',\n  bar: 42,\n' +
                     '  zoo: true,\n  metadata: undefined\n' +
                     '}\n');
  assert.strictEqual(result.code, 0);
});

test('expect error when executing a TypeScript file with generics', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-parameter-properties.ts'),
  ]);

  // This error should be thrown during transformation
  assert.match(
    result.stderr,
    /TypeScript parameter property is not supported in strip-only mode/
  );
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript loader and a .ts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--import',
    fixtures.fileURL('typescript/ts/test-loader.ts'),
    fixtures.path('typescript/ts/test-typescript.ts'),
  ]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a TypeScript loader and a .js file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--import',
    fixtures.fileURL('typescript/ts/test-loader.ts'),
    fixtures.path('typescript/ts/test-simple.js'),
  ]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute invalid TypeScript syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/ts/test-invalid-syntax.ts'),
  ]);

  assert.match(result.stderr, /ERR_INVALID_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('check transform types warning', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    fixtures.path('typescript/ts/test-typescript.ts'),
  ]);

  assert.match(result.stderr, /Transform Types is an experimental feature and might change at any time/);
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('expect error when executing a TypeScript file with --jitless', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--jitless',
    fixtures.path('typescript/ts/test-typescript.ts'),
  ]);

  assert.match(result.stderr, /ERR_WEBASSEMBLY_NOT_SUPPORTED/);
  assert.match(result.stderr, /WebAssembly is not supported in this environment, but is required for TypeScript/);
  assert.strictEqual(result.code, 1);
});
