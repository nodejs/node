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
  }
}

assert.throws(() => new Module(), /INVALID_ARG_TYPE/);

rejects(async () => {
  const m = new Module('');
  await m.link();
  assert.strictEqual(m.linkingStatus, 'linked');
  await m.link();
}, /MODULE_ALREADY_LINKED/);

rejects(async () => {
  const m = new Module('');
  m.link();
  assert.strictEqual(m.linkingStatus, 'linking');
  await m.link();
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
  await foo.link(() => {});
  const bar = new Module('import "foo";');
  try {
    await bar.link(() => foo);
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
  await m.link(async () => {
    assert.strictEqual(m.linkingStatus, 'linking');
    assert.throws(() => {
      m.instantiate();
    }, /MODULE_NOT_LINKED/);
    const sub = new Module('');
    await sub.link(() => {});
    return sub;
  });
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
  await m.link();
  await m.evaluate();
}, /MODULE_STATUS/);

assert.throws(() => {
  const m = new Module('');
  m.error;
}, /MODULE_STATUS/);

rejects(async () => {
  const m = new Module('');
  await m.link();
  await m.evaluate();
  m.error;
}, /MODULE_STATUS/);

assert.throws(() => {
  const m = new Module('');
  m.namespace;
}, /MODULE_STATUS/);

rejects(async () => {
  const m = new Module('');
  await m.link();
  m.namespace;
}, /MODULE_STATUS/);

(async () => {
  const m = new Module('throw new Error();');
  await m.link();
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
