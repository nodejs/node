// Flags: --experimental-import-meta-resolve
import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { execPath } from 'node:process';

assert.deepStrictEqual(await import.meta.node.resolveURL('./test-esm-import-meta.mjs'),
                       new URL('./test-esm-import-meta.mjs', import.meta.url));
assert.deepStrictEqual(await import.meta.node.resolveURL(new URL(import.meta.url)),
                       new URL(import.meta.url));
{
  // Testing with specifiers that does not lead to actual modules:
  const notFound = await import.meta.node.resolveURL('./notfound.mjs');
  assert.deepStrictEqual(notFound, new URL('./notfound.mjs', import.meta.url));
  const noExtension = await import.meta.node.resolveURL('./asset');
  assert.deepStrictEqual(noExtension, new URL('./asset', import.meta.url));
  await assert.rejects(import.meta.node.resolveURL('does-not-exist'), { code: 'ERR_MODULE_NOT_FOUND' });
}
assert.strictEqual(
  `${await import.meta.node.resolveURL('../fixtures/empty-with-bom.txt')}`,
  import.meta.resolve('../fixtures/empty-with-bom.txt'));
assert.deepStrictEqual(
  await import.meta.node.resolveURL('../fixtures/empty-with-bom.txt'),
  fixtures.fileURL('empty-with-bom.txt'));
assert.deepStrictEqual(
  await import.meta.node.resolveURL('./empty-with-bom.txt', fixtures.fileURL('./')),
  fixtures.fileURL('empty-with-bom.txt'));
assert.deepStrictEqual(
  await import.meta.node.resolveURL('./empty-with-bom.txt', fixtures.fileURL('./').href),
  fixtures.fileURL('empty-with-bom.txt'));
await [[], {}, Symbol(), 0, 1, 1n, 1.1, () => {}, true, false].map((arg) =>
  assert.rejects(import.meta.node.resolveURL('../fixtures/', arg), {
    code: 'ERR_INVALID_ARG_TYPE',
  })
);
assert.deepStrictEqual(await import.meta.node.resolveURL('http://some-absolute/url'), new URL('http://some-absolute/url'));
assert.deepStrictEqual(await import.meta.node.resolveURL('some://weird/protocol'), new URL('some://weird/protocol'));
assert.deepStrictEqual(await import.meta.node.resolveURL('baz/', fixtures.fileURL('./')),
                       fixtures.fileURL('node_modules/baz/'));

await Promise.all([

  async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'console.log(typeof import.meta.node.resolveURL)',
    ]);
    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'function\n');
    assert.strictEqual(code, 0);
  },

  async () => {
    const cp = spawn(execPath, [
      '--input-type=module',
    ]);
    cp.stdin.end('console.log(typeof import.meta.node.resolveURL)');
    assert.match((await cp.stdout.toArray()).toString(), /^function\r?\n$/);
  },

  async () => {
    // Should return a Promise.
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "data:text/javascript,console.log(import.meta.node.resolveURL(%22node:os%22))"',
    ]);
    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'Promise { <pending> }\n');
    assert.strictEqual(code, 0);
  },

  async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "data:text/javascript,console.log(`%24{await import.meta.node.resolveURL(%22node:os%22)}`)"',
    ]);
    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'node:os\n');
    assert.strictEqual(code, 0);
  },

  async () => {
    // Should be available in custom loaders.
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,console.log(`%24{await import.meta.node.resolveURL(%22node:os%22)}`)',
      '--eval',
      'setTimeout(()=>{}, 99)',
    ]);
    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'node:os\n');
    assert.strictEqual(code, 0);
  },

  async () => {
    const cp = spawn(execPath, [
      '--input-type=module',
    ]);
    cp.stdin.end('import "data:text/javascript,console.log(`%24{await import.meta.node.resolveURL(%22node:os%22)}`)"');
    assert.match((await cp.stdout.toArray()).toString(), /^node:os\r?\n$/);
  },

].map((fn) => fn()));
