// Flags: --experimental-import-meta-resolve
import '../common/index.mjs';
import assert from 'assert';
import { spawn } from 'child_process';
import { execPath } from 'process';

const dirname = import.meta.url.slice(0, import.meta.url.lastIndexOf('/') + 1);
const fixtures = dirname.slice(0, dirname.lastIndexOf('/', dirname.length - 2) + 1) + 'fixtures/';

assert.strictEqual(import.meta.resolve('./test-esm-import-meta.mjs'),
                   dirname + 'test-esm-import-meta.mjs');
try {
  import.meta.resolve('./notfound.mjs');
  assert.fail();
} catch (e) {
  assert.strictEqual(e.code, 'ERR_MODULE_NOT_FOUND');
}
assert.strictEqual(
  import.meta.resolve('../fixtures/empty-with-bom.txt'),
  fixtures + 'empty-with-bom.txt');
assert.strictEqual(import.meta.resolve('../fixtures/'), fixtures);
assert.strictEqual(
  import.meta.resolve('../fixtures/', import.meta.url),
  fixtures);
assert.strictEqual(
  import.meta.resolve('../fixtures/', new URL(import.meta.url)),
  fixtures);
[[], {}, Symbol(), 0, 1, 1n, 1.1, () => {}, true, false].map((arg) =>
  assert.throws(() => import.meta.resolve('../fixtures/', arg), {
    code: 'ERR_INVALID_ARG_TYPE',
  })
);
assert.strictEqual(import.meta.resolve('baz/', fixtures),
                   fixtures + 'node_modules/baz/');

{
  const cp = spawn(execPath, [
    '--experimental-import-meta-resolve',
    '--input-type=module',
    '--eval', 'console.log(typeof import.meta.resolve)',
  ]);
  assert.match((await cp.stdout.toArray()).toString(), /^function\r?\n$/);
}

{
  const cp = spawn(execPath, [
    '--experimental-import-meta-resolve',
    '--input-type=module',
  ]);
  cp.stdin.end('console.log(typeof import.meta.resolve)');
  assert.match((await cp.stdout.toArray()).toString(), /^function\r?\n$/);
}

{
  const cp = spawn(execPath, [
    '--experimental-import-meta-resolve',
    '--input-type=module',
    '--eval', 'import "data:text/javascript,console.log(import.meta.resolve(%22node:os%22))"',
  ]);
  assert.match((await cp.stdout.toArray()).toString(), /^node:os\r?\n$/);
}

{
  const cp = spawn(execPath, [
    '--experimental-import-meta-resolve',
    '--input-type=module',
  ]);
  cp.stdin.end('import "data:text/javascript,console.log(import.meta.resolve(%22node:os%22))"');
  assert.match((await cp.stdout.toArray()).toString(), /^node:os\r?\n$/);
}
