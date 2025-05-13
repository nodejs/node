// Flags: --experimental-vm-modules
'use strict';

const common = require('../common');

const assert = require('assert');
const { createContext, Script, SourceTextModule } = require('vm');

// Verifies that a `import` call returns a promise created in the context
// where the `import` was called, not the context of `importModuleDynamically`
// callback.

async function testScript() {
  const ctx = createContext();

  const mod1 = new SourceTextModule('export const a = 1;', {
    context: ctx,
  });
  // No import statements, so must not link statically.
  await mod1.link(common.mustNotCall());

  const script2 = new Script(`
    const promise = import("mod1");
    if (Object.getPrototypeOf(promise) !== Promise.prototype) {
      throw new Error('Expected promise to be a Promise');
    }
    globalThis.__result = promise;
  `, {
    importModuleDynamically: common.mustCall((specifier, referrer) => {
      assert.strictEqual(specifier, 'mod1');
      assert.strictEqual(referrer, script2);
      return mod1;
    }),
  });
  script2.runInContext(ctx);

  // Wait for the promise to resolve.
  await ctx.__result;
}

async function testScriptImportFailed() {
  const ctx = createContext();

  const mod1 = new SourceTextModule('export const a = 1;', {
    context: ctx,
  });
  // No import statements, so must not link statically.
  await mod1.link(common.mustNotCall());

  const script2 = new Script(`
    const promise = import("mod1");
    if (Object.getPrototypeOf(promise) !== Promise.prototype) {
      throw new Error('Expected promise to be a Promise');
    }
    globalThis.__result = promise;
  `, {
    importModuleDynamically: common.mustCall((specifier, referrer) => {
      throw new Error('import failed');
    }),
  });
  script2.runInContext(ctx);

  // Wait for the promise to reject.
  await assert.rejects(ctx.__result, {
    message: 'import failed',
  });
}

async function testModule() {
  const ctx = createContext();

  const mod1 = new SourceTextModule('export const a = 1;', {
    context: ctx,
  });
  // No import statements, so must not link statically.
  await mod1.link(common.mustNotCall());

  const mod2 = new SourceTextModule(`
    const promise = import("mod1");
    if (Object.getPrototypeOf(promise) !== Promise.prototype) {
      throw new Error('Expected promise to be a Promise');
    }
    await promise;
  `, {
    context: ctx,
    importModuleDynamically: common.mustCall((specifier, referrer) => {
      assert.strictEqual(specifier, 'mod1');
      assert.strictEqual(referrer, mod2);
      return mod1;
    }),
  });
  // No import statements, so must not link statically.
  await mod2.link(common.mustNotCall());
  await mod2.evaluate();
}

async function testModuleImportFailed() {
  const ctx = createContext();

  const mod1 = new SourceTextModule('export const a = 1;', {
    context: ctx,
  });
  // No import statements, so must not link statically.
  await mod1.link(common.mustNotCall());

  const mod2 = new SourceTextModule(`
    const promise = import("mod1");
    if (Object.getPrototypeOf(promise) !== Promise.prototype) {
      throw new Error('Expected promise to be a Promise');
    }
    await promise.then(() => {
      throw new Error('Expected promise to be rejected');
    }, (e) => {
      if (e.message !== 'import failed') {
        throw new Error('Expected promise to be rejected with "import failed"');
      }
    });
  `, {
    context: ctx,
    importModuleDynamically: common.mustCall((specifier, referrer) => {
      throw new Error('import failed');
    }),
  });
  // No import statements, so must not link statically.
  await mod2.link(common.mustNotCall());
  await mod2.evaluate();
}

Promise.all([
  testScript(),
  testScriptImportFailed(),
  testModule(),
  testModuleImportFailed(),
]).then(common.mustCall());
