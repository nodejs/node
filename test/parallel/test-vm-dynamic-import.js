'use strict';

// Flags: --experimental-vm-modules --experimental-modules

const common = require('../common');
const assert = require('assert');
common.crashOnUnhandledRejection();

const vm = require('vm');

async function testModule() {
  {
    const module = new vm.Module('import("X")');
    await module.link(common.mustCall((specifier) => {
      assert.strictEqual(specifier, 'X');
      return new vm.Module('export const x = 5');
    }));
    module.instantiate();
    const { result } = await module.evaluate();
    assert.strictEqual((await result).x, 5);
  }

  {
    const module = new vm.Module('import("X")');
    await module.link(common.mustCall(async (specifier) => {
      const m = new vm.Module('export const x = 5');
      await m.link(() => undefined);
      m.instantiate();
      await m.evaluate();
      return m.namespace;
    }));
    module.instantiate();
    const { result } = await module.evaluate();
    assert.strictEqual((await result).x, 5);
  }
}

async function testScript() {
  let moduleX;
  const script = new vm.Script('import("vm://X")', {
    resolveDynamicImport(specifier, scriptOrModule) {
      if (specifier === 'vm://X') {
        assert.strictEqual(scriptOrModule, script);
        return moduleX = new vm.Module('export default import("vm://Y")', { url: 'vm://X' });
      }
      if (specifier === 'vm://Y') {
        assert.strictEqual(scriptOrModule, moduleX);
        return new vm.Module('export default 5', { url: 'vm://Y' });
      }
    },
  });
  const result = script.runInThisContext();
  assert.strictEqual((await (await result).default).default, 5);
}

async function testNewContext() {
  const context = vm.createContext();
  const module = new vm.Module('import("X")', {
    context,
    url: 'vm:new_context_test.js',
  });
  await module.link(common.mustCall((specifier) => {
    assert.strictEqual(specifier, 'X');
    return new vm.Module('export const x = 5', { context });
  }));
  module.instantiate();
  const { result } = await module.evaluate();
  assert.strictEqual((await result).x, 5);
}

async function testFail() {
  await assert.rejects(() => {
    const m = new vm.Script('import("X")');
    return m.runInThisContext();
  }, common.expectsError({
    code: 'ERR_DYNAMIC_IMPORT_CALLBACK_MISSING',
  }));

  await assert.rejects(() => {
    const m = new vm.Script('import("X")', {
      resolveDynamicImport() {
        return 5;
      }
    });
    return m.runInThisContext();
  }, common.expectsError({
    code: 'ERR_VM_MODULE_NOT_MODULE',
  }));

  await assert.rejects(() => {
    const script = new vm.Script('import("X")', {
      resolveDynamicImport: common.mustCall(() => ({})),
    });
    return script.runInThisContext();
  }, common.expectsError({
    code: 'ERR_VM_MODULE_NOT_MODULE',
  }));
}

(async () => {
  await testModule();
  await testScript();
  await testNewContext();
  await testFail();
})();
