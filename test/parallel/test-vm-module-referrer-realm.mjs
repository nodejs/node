// Flags: --experimental-vm-modules
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { Script, SourceTextModule, createContext } from 'node:vm';

/**
 * This test verifies that dynamic import in an indirect eval without JS stacks.
 * In this case, the referrer for the dynamic import will be null and the
 * per-context importModuleDynamically callback will be invoked.
 *
 * Caveat: this test can be unstable if the loader internals are changed and performs
 * microtasks with a JS stack (e.g. with CallbackScope). In this case, the
 * referrer will be resolved to the top JS stack frame `node:internal/process/task_queues.js`.
 * This is due to the implementation detail of how V8 finds the referrer for a dynamic import
 * call.
 */

async function test() {
  const foo = new SourceTextModule('export const a = 1;');
  await foo.link(common.mustNotCall());
  await foo.evaluate();

  const ctx = createContext({}, {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      assert.strictEqual(specifier, 'foo');
      assert.strictEqual(wrap, ctx);
      return foo;
    }, 2),
  });
  {
    const s = new Script('Promise.resolve("import(\'foo\')").then(eval)', {
      importModuleDynamically: common.mustNotCall(),
    });

    const result = s.runInContext(ctx);
    assert.strictEqual(await result, foo.namespace);
  }

  {
    const m = new SourceTextModule('globalThis.fooResult = Promise.resolve("import(\'foo\')").then(eval)', {
      context: ctx,
      importModuleDynamically: common.mustNotCall(),
    });
    await m.link(common.mustNotCall());
    await m.evaluate();
    assert.strictEqual(await ctx.fooResult, foo.namespace);
    delete ctx.fooResult;
  }
}

async function testMissing() {
  const ctx = createContext({});
  {
    const s = new Script('Promise.resolve("import(\'foo\')").then(eval)', {
      importModuleDynamically: common.mustNotCall(),
    });

    const result = s.runInContext(ctx);
    await assert.rejects(result, {
      code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING',
    });
  }

  {
    const m = new SourceTextModule('globalThis.fooResult = Promise.resolve("import(\'foo\')").then(eval)', {
      context: ctx,
      importModuleDynamically: common.mustNotCall(),
    });
    await m.link(common.mustNotCall());
    await m.evaluate();

    await assert.rejects(ctx.fooResult, {
      code: 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING',
    });
    delete ctx.fooResult;
  }
}

await Promise.all([
  test(),
  testMissing(),
]).then(common.mustCall());
