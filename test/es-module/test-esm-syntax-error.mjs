import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: importing a module with syntax error(s)', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should throw', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      fixtures.path('es-module-loaders', 'syntax-error.mjs'),
    ]);
    assert.match(stderr, /SyntaxError:/);
    assert.notStrictEqual(code, 0);
  });

  it('should include the source location when imported dynamically', async () => {
    const moduleURL = fixtures.fileURL('es-module-loaders', 'syntax-error.mjs');
    const { code, stderr } = await spawnPromisified(execPath, [
      '-e',
      `import(${JSON.stringify(moduleURL.href)}).catch(console.error)`,
    ]);

    assert.strictEqual(code, 0);
    assert.match(stderr, /syntax-error\.mjs:2/);
    assert.match(stderr, /await async \(\) => 0;\n\^+/);
  });
});
