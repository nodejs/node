'use strict';

// Flags: --experimental-vm-modules --experimental-modules

const common = require('../common');

const assert = require('assert');
const { Script, SourceTextModule, createContext } = require('vm');

async function testNoCallback() {
  const m = new SourceTextModule('import("foo")', { context: createContext() });
  await m.link(common.mustNotCall());
  m.instantiate();
  const { result } = await m.evaluate();
  let threw = false;
  try {
    await result;
  } catch (err) {
    threw = true;
    assert.strictEqual(err.code, 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING');
  }
  assert(threw);
}

async function test() {
  const foo = new SourceTextModule('export const a = 1;');
  await foo.link(common.mustNotCall());
  foo.instantiate();
  await foo.evaluate();

  {
    const s = new Script('import("foo")', {
      importModuleDynamically: common.mustCall((specifier, wrap) => {
        assert.strictEqual(specifier, 'foo');
        assert.strictEqual(wrap, s);
        return foo;
      }),
    });

    const result = s.runInThisContext();
    assert.strictEqual(foo.namespace, await result);
  }

  {
    const m = new SourceTextModule('import("foo")', {
      importModuleDynamically: common.mustCall((specifier, wrap) => {
        assert.strictEqual(specifier, 'foo');
        assert.strictEqual(wrap, m);
        return foo;
      }),
    });
    await m.link(common.mustNotCall());
    m.instantiate();
    const { result } = await m.evaluate();
    assert.strictEqual(foo.namespace, await result);
  }
}

async function testInvalid() {
  const m = new SourceTextModule('import("foo")', {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      return 5;
    }),
  });
  await m.link(common.mustNotCall());
  m.instantiate();
  const { result } = await m.evaluate();
  await result.catch(common.mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_VM_MODULE_NOT_MODULE');
  }));

  const s = new Script('import("foo")', {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      return undefined;
    }),
  });
  let threw = false;
  try {
    await s.runInThisContext();
  } catch (e) {
    threw = true;
    assert.strictEqual(e.code, 'ERR_VM_MODULE_NOT_MODULE');
  }
  assert(threw);
}

async function testInvalidimportModuleDynamically() {
  assert.throws(
    () => new Script(
      'import("foo")',
      { importModuleDynamically: false }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}

(async function() {
  await testNoCallback();
  await test();
  await testInvalid();
  await testInvalidimportModuleDynamically();
}()).then(common.mustCall());
