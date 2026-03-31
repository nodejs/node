import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

const urlToRegister = fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs');
const urlToRegisterEscaped = JSON.stringify(urlToRegister.href);


describe('module.register() experimental warning', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('emits ExperimentalWarning when module.register() is called', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module'; register(${urlToRegisterEscaped});`,
    ]);

    const expectedWarning = 'ExperimentalWarning: `module.register()` is an experimental feature and will be ' +
      'removed in a future version of Node.js. Use `module.registerHooks()` instead.';
    assert.ok(stderr.includes(expectedWarning));
    assert.strictEqual(code, 0);
  });

  it('only emits the warning once per process', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module';
       register(${urlToRegisterEscaped});
       register(${urlToRegisterEscaped});`,
    ]);

    assert.strictEqual(stderr.split('module.register()').length - 1, 1);
    assert.strictEqual(code, 0);
  });

  it('does not emit when --no-warnings is set', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module'; register(${urlToRegisterEscaped});`,
    ]);

    assert.doesNotMatch(stderr, /ExperimentalWarning/);
    assert.strictEqual(code, 0);
  });
});
