'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');
const { Script, SourceTextModule } = require('vm');

async function testNoCallback() {
  const m = new SourceTextModule(`
    globalThis.importResult = import("foo");
    globalThis.importResult.catch(() => {});
  `);
  await m.link(common.mustNotCall());
  await m.evaluate();
  let threw = false;
  try {
    await globalThis.importResult;
  } catch (err) {
    threw = true;
    assert.strictEqual(err.code, 'ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING');
  }
  delete globalThis.importResult;
  assert(threw);
}

async function test() {
  const foo = new SourceTextModule('export const a = 1;');
  await foo.link(common.mustNotCall());
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
    assert.strictEqual(await result, foo.namespace);
  }

  {
    const m = new SourceTextModule('globalThis.fooResult = import("foo")', {
      importModuleDynamically: common.mustCall((specifier, wrap) => {
        assert.strictEqual(specifier, 'foo');
        assert.strictEqual(wrap, m);
        return foo;
      }),
    });
    await m.link(common.mustNotCall());
    await m.evaluate();
    assert.strictEqual(await globalThis.fooResult, foo.namespace);
    delete globalThis.fooResult;
  }

  {
    const s = new Script('import("foo", { with: { key: "value" } })', {
      importModuleDynamically: common.mustCall((specifier, wrap, attributes) => {
        assert.strictEqual(specifier, 'foo');
        assert.strictEqual(wrap, s);
        assert.deepStrictEqual(attributes, { __proto__: null, key: 'value' });
        return foo;
      }),
    });

    const result = s.runInThisContext();
    assert.strictEqual(await result, foo.namespace);
  }
}

async function testInvalid() {
  const m = new SourceTextModule('globalThis.fooResult = import("foo")', {
    importModuleDynamically: common.mustCall((specifier, wrap) => {
      return 5;
    }),
  });
  await m.link(common.mustNotCall());
  await m.evaluate();
  await globalThis.fooResult.catch(common.mustCall((e) => {
    assert.strictEqual(e.code, 'ERR_VM_MODULE_NOT_MODULE');
  }));
  delete globalThis.fooResult;

  const s = new Script('import("bar")', {
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
