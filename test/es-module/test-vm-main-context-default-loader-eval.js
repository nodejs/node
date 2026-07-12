'use strict';
/**
 * This test verifies that dynamic import in an indirect eval without JS stacks.
 * In this case, the referrer for the dynamic import will be null and the main
 * context default loader in createContext() resolves to cwd.
 *
 * Caveat: this test can be unstable if the loader internals are changed and performs
 * microtasks with a JS stack (e.g. with CallbackScope). In this case, the
 * referrer will be resolved to the top JS stack frame `node:internal/process/task_queues.js`.
 * This is due to the implementation detail of how V8 finds the referrer for a dynamic import
 * call.
 */

const common = require('../common');

// Can't process.chdir() in worker.
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('node:fs');
const {
  Script,
  createContext,
  constants: { USE_MAIN_CONTEXT_DEFAULT_LOADER },
} = require('node:vm');
const assert = require('node:assert');

common.expectWarning('ExperimentalWarning',
                     'vm.USE_MAIN_CONTEXT_DEFAULT_LOADER is an experimental feature and might change at any time');
assert(
  !process.execArgv.includes('--experimental-vm-modules'),
  'This test must be run without --experimental-vm-modules');
assert.strictEqual(typeof USE_MAIN_CONTEXT_DEFAULT_LOADER, 'symbol');

async function main() {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  //
  {
    const options = {
      importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
    };
    const ctx = createContext({}, options);
    const s = new Script('Promise.resolve("import(\'./message.mjs\')").then(eval)', {
      importModuleDynamically: common.mustNotCall(),
    });
    await assert.rejects(s.runInContext(ctx), { code: 'ERR_MODULE_NOT_FOUND' });
  }

  const moduleUrl = fixtures.fileURL('es-modules', 'message.mjs');
  fs.copyFileSync(moduleUrl, tmpdir.resolve('message.mjs'));
  {
    const options = {
      importModuleDynamically: USE_MAIN_CONTEXT_DEFAULT_LOADER,
    };
    const ctx = createContext({}, options);
    const moduleUrl = fixtures.fileURL('es-modules', 'message.mjs');
    const namespace = await import(moduleUrl.href);
    const script = new Script('Promise.resolve("import(\'./message.mjs\')").then(eval)', {
      importModuleDynamically: common.mustNotCall(),
    });
    const result = await script.runInContext(ctx);
    assert.deepStrictEqual(result, namespace);
  }
}

main().then(common.mustCall());
