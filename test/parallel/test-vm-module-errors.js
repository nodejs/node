'use strict';

// Flags: --experimental-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');

const { Module, createContext } = require('vm');

async function rejects(fn, validateError) {
  try {
    await fn();
  } catch (err) {
    validateError(err);
    return;
  }
  assert.fail('Missing expected exception');
}

async function createEmptyModule() {
  const m = new Module('');
  await m.link(common.mustNotCall());
  return m;
}

async function checkArgType() {
  assert.throws(() => {
    new Module();
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }));

  for (const invalidOptions of [
    0, 1, null, true, 'str', () => {}, Symbol.iterator
  ]) {
    assert.throws(() => {
      new Module('', invalidOptions);
    }, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }));
  }

  for (const invalidLinker of [
    0, 1, undefined, null, true, 'str', {}, Symbol.iterator
  ]) {
    await rejects(async () => {
      const m = new Module('');
      await m.link(invalidLinker);
    }, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }));
  }
}

// Check methods/properties can only be used under a specific state.
async function checkModuleState() {
  await rejects(async () => {
    const m = new Module('');
    await m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linked');
    await m.link(common.mustNotCall());
  }, common.expectsError({
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  }));

  await rejects(async () => {
    const m = new Module('');
    m.link(common.mustNotCall());
    assert.strictEqual(m.linkingStatus, 'linking');
    await m.link(common.mustNotCall());
  }, common.expectsError({
    code: 'ERR_VM_MODULE_ALREADY_LINKED'
  }));

  assert.throws(() => {
    const m = new Module('');
    m.instantiate();
  }, common.expectsError({
    code: 'ERR_VM_MODULE_NOT_LINKED'
  }));

  await rejects(async () => {
    const m = new Module('import "foo";');
    try {
      await m.link(common.mustCall(() => ({})));
    } catch (err) {
      assert.strictEqual(m.linkingStatus, 'errored');
      m.instantiate();
    }
    assert.fail('Unreachable');
  }, common.expectsError({
    code: 'ERR_VM_MODULE_NOT_LINKED'
  }));

  {
    const m = new Module('import "foo";');
    await m.link(common.mustCall(async (module, specifier) => {
      assert.strictEqual(module, m);
      assert.strictEqual(specifier, 'foo');
      assert.strictEqual(m.linkingStatus, 'linking');
      assert.throws(() => {
        m.instantiate();
      }, common.expectsError({
        code: 'ERR_VM_MODULE_NOT_LINKED'
      }));
      return createEmptyModule();
    }));
    m.instantiate();
    await m.evaluate();
  }

  await rejects(async () => {
    const m = new Module('');
    await m.evaluate();
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  }));

  await rejects(async () => {
    const m = await createEmptyModule();
    await m.evaluate();
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be one of instantiated, evaluated, and errored'
  }));

  assert.throws(() => {
    const m = new Module('');
    m.error;
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be errored'
  }));

  await rejects(async () => {
    const m = await createEmptyModule();
    m.instantiate();
    await m.evaluate();
    m.error;
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must be errored'
  }));

  assert.throws(() => {
    const m = new Module('');
    m.namespace;
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must not be uninstantiated or instantiating'
  }));

  await rejects(async () => {
    const m = await createEmptyModule();
    m.namespace;
  }, common.expectsError({
    code: 'ERR_VM_MODULE_STATUS',
    message: 'Module status must not be uninstantiated or instantiating'
  }));
}

// Check link() fails when the returned module is not valid.
async function checkLinking() {
  await rejects(async () => {
    const m = new Module('import "foo";');
    try {
      await m.link(common.mustCall(() => ({})));
    } catch (err) {
      assert.strictEqual(m.linkingStatus, 'errored');
      throw err;
    }
    assert.fail('Unreachable');
  }, common.expectsError({
    code: 'ERR_VM_MODULE_NOT_MODULE'
  }));

  await rejects(async () => {
    const c = createContext({ a: 1 });
    const foo = new Module('', { context: c });
    await foo.link(common.mustNotCall());
    const bar = new Module('import "foo";');
    try {
      await bar.link(common.mustCall(() => foo));
    } catch (err) {
      assert.strictEqual(bar.linkingStatus, 'errored');
      throw err;
    }
    assert.fail('Unreachable');
  }, common.expectsError({
    code: 'ERR_VM_MODULE_DIFFERENT_CONTEXT'
  }));

  // await rejects(async () => {
  //   const m = new Module('import "foo";');
  //   await m.link(common.mustCall(() => new Module('')));
  // }, common.expectsError({
  //   code: 'ERR_VM_MODULE_NOT_LINKED'
  // }));
}

// Check the JavaScript engine deals with exceptions correctly
async function checkExecution() {
  await (async () => {
    const m = new Module('import { nonexistent } from "module";');
    await m.link(common.mustCall(createEmptyModule));

    // There is no code for this exception since it is thrown by the JavaScript
    // engine.
    assert.throws(() => {
      m.instantiate();
    }, SyntaxError);
  })();

  await (async () => {
    const m = new Module('throw new Error();');
    await m.link(common.mustNotCall());
    m.instantiate();
    const evaluatePromise = m.evaluate();
    await evaluatePromise.catch(() => {});
    assert.strictEqual(m.status, 'errored');
    try {
      await evaluatePromise;
    } catch (err) {
      assert.strictEqual(m.error, err);
      return;
    }
    assert.fail('Missing expected exception');
  })();
}

async function main() {
  await checkArgType();
  await checkModuleState();
  await checkLinking();
  await checkExecution();
}

main();
