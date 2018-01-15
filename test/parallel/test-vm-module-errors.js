'use strict';

// Flags: --experimental-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');

const { Module, createContext } = require('vm');

async function rejects(fn, re) {
  try {
    await fn();
  } catch (err) {
    const full = `${err.code || ''} ${err.message}`.trim();
    if (/An invalid error message key was used/.test(full))
      assert.fail(full);
    else if (!re.test(full))
      assert.fail(`'${full}' did not match regex ${re}`);
    else
      return;
  }
  assert.fail('No exception detected');
}

assert.throws(() => new Module(), /INVALID_ARG_TYPE/);

const notFunctions = [0, 1, undefined, null, true, 'str', {}, Symbol.iterator];

for (const invalidLinker of notFunctions) {
  rejects(async () => {
    const m = new Module('');
    await m.link(notFunctions);
  }, /INVALID_ARG_TYPE/);
}

rejects(async () => {
  const m = new Module('');
  await m.link(common.mustNotCall());
  assert.strictEqual(m.linkingStatus, 'linked');
  await m.link(common.mustNotCall());
}, /MODULE_ALREADY_LINKED/);

rejects(async () => {
  const m = new Module('');
  m.link(common.mustNotCall());
  assert.strictEqual(m.linkingStatus, 'linking');
  await m.link(common.mustNotCall());
}, /MODULE_ALREADY_LINKED/);

rejects(async () => {
  const m = new Module('import "foo";');
  try {
    await m.link(() => ({}));
  } catch (err) {
    assert.strictEqual(m.linkingStatus, 'errored');
    throw err;
  }
  assert.fail('Unreachable');
}, /MODULE_NOT_MODULE/);

rejects(async () => {
  const c = createContext({ a: 1 });
  const foo = new Module('', { context: c });
  await foo.link(common.mustNotCall());
  const bar = new Module('import "foo";');
  try {
    await bar.link(common.mustCall(() => foo, 1));
  } catch (err) {
    assert.strictEqual(bar.linkingStatus, 'errored');
    throw err;
  }
  assert.fail('Unreachable');
}, /MODULE_DIFFERENT_CONTEXT/);

assert.throws(() => {
  const m = new Module('');
  m.instantiate();
}, /MODULE_NOT_LINKED/);

(async () => {
  const m = new Module('import "foo";');
  await m.link(common.mustCall(async (module, specifier) => {
    assert.strictEqual(module, m);
    assert.strictEqual(specifier, 'foo');
    assert.strictEqual(m.linkingStatus, 'linking');
    assert.throws(() => {
      m.instantiate();
    }, /MODULE_NOT_LINKED/);
    const sub = new Module('');
    await sub.link(common.mustNotCall());
    return sub;
  }, 1));
  m.instantiate();
  await m.evaluate();
})();

rejects(async () => {
  const m = new Module('import "foo";');
  try {
    await m.link(() => ({}));
  } catch (err) {
    assert.strictEqual(m.linkingStatus, 'errored');
    m.instantiate();
  }
  assert.fail('Unreachable');
}, /MODULE_NOT_LINKED/);

rejects(async () => {
  const m = new Module('');
  await m.evaluate();
}, /MODULE_STATUS/);

rejects(async () => {
  const m = new Module('');
  await m.link(common.mustNotCall());
  await m.evaluate();
}, /MODULE_STATUS/);

assert.throws(() => {
  const m = new Module('');
  m.error;
}, /MODULE_STATUS/);

rejects(async () => {
  const m = new Module('');
  await m.link(common.mustNotCall());
  await m.evaluate();
  m.error;
}, /MODULE_STATUS/);

assert.throws(() => {
  const m = new Module('');
  m.namespace;
}, /MODULE_STATUS/);

rejects(async () => {
  const m = new Module('');
  await m.link(common.mustNotCall());
  m.namespace;
}, /MODULE_STATUS/);

(async () => {
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
  }
})();
