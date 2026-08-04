import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

const urlToRegister = fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs');
const urlToRegisterEscaped = JSON.stringify(urlToRegister.href);


describe('module.register() deprecation (DEP0205)', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('emits DEP0205 when module.register() is called', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module'; register(${urlToRegisterEscaped});`,
    ]);

    assert.match(stderr, /\[DEP0205\]/);
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

    assert.strictEqual(stderr.split('[DEP0205]').length - 1, 1);
    assert.strictEqual(code, 0);
  });

  it('does not emit when --no-deprecation is set', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--no-deprecation',
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module'; register(${urlToRegisterEscaped});`,
    ]);

    assert.doesNotMatch(stderr, /DEP0205/);
    assert.strictEqual(code, 0);
  });

  it('throws when --throw-deprecation is set', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--throw-deprecation',
      '--input-type=module',
      '--eval',
      `import { register } from 'node:module'; register(${urlToRegisterEscaped});`,
    ]);

    assert.match(stderr, /DEP0205/);
    assert.notStrictEqual(code, 0);
  });
});
