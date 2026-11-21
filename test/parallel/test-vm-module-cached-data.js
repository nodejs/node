'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');
const { SourceTextModule, createContext } = require('vm');

{
  const m = new SourceTextModule('const a = 1');
  const cachedData = m.createCachedData();

  new SourceTextModule('const a = 1', { cachedData });

  assert.throws(() => {
    new SourceTextModule('differentSource', { cachedData });
  }, {
    code: 'ERR_VM_MODULE_CACHED_DATA_REJECTED',
  });
}

// A code cache that is created after evaluation is accepted by a new module
// in another context.
(async () => {
  const source = 'export function add(a, b) { return a + b; }\n' +
                 'export const sum = add(1, 2);\n';
  const m = new SourceTextModule(source);
  await m.link(common.mustNotCall());
  await m.evaluate();
  assert.strictEqual(m.status, 'evaluated');
  const cachedData = m.createCachedData();
  assert(cachedData.length > 0);

  const m2 = new SourceTextModule(source, {
    context: createContext(),
    cachedData,
  });
  await m2.link(common.mustNotCall());
  await m2.evaluate();
  assert.strictEqual(m2.namespace.sum, 3);
  assert.strictEqual(m2.namespace.add(2, 3), 5);
})().then(common.mustCall());

// A code cache that is created after an exported function was called is
// accepted by a new module.
(async () => {
  const source = 'export function work(n) {\n' +
                 '  let total = 0;\n' +
                 '  for (let i = 0; i < n; i++) {\n' +
                 '    total += i * 2;\n' +
                 '  }\n' +
                 '  return total;\n' +
                 '}\n';
  const m = new SourceTextModule(source);
  await m.link(common.mustNotCall());
  await m.evaluate();
  assert.strictEqual(m.namespace.work(4), 12);
  const after = m.createCachedData();

  const m2 = new SourceTextModule(source, { cachedData: after });
  await m2.link(common.mustNotCall());
  await m2.evaluate();
  assert.strictEqual(m2.namespace.work(4), 12);
})().then(common.mustCall());

// A code cache can be created while a top-level await is pending.
(async () => {
  const { promise, resolve } = Promise.withResolvers();
  const source = 'export function later() { return 42; }\n' +
                 'await pending;\n';
  const m = new SourceTextModule(source, {
    context: createContext({ pending: promise }),
  });
  await m.link(common.mustNotCall());
  const evaluation = m.evaluate();
  // V8 reports a pending top-level await as evaluated
  assert.strictEqual(m.status, 'evaluated');
  const cachedData = m.createCachedData();
  resolve();
  await evaluation;

  const m2 = new SourceTextModule(source, {
    context: createContext({ pending: Promise.resolve() }),
    cachedData,
  });
  await m2.link(common.mustNotCall());
  await m2.evaluate();
  assert.strictEqual(m2.namespace.later(), 42);
})().then(common.mustCall());

// A code cache can be created while a top-level await is pending, but not
// after the awaited promise rejects.
(async () => {
  const { promise, reject } = Promise.withResolvers();
  const m = new SourceTextModule('await pending;', {
    context: createContext({ pending: promise }),
  });
  await m.link(common.mustNotCall());
  const evaluation = m.evaluate();
  assert.strictEqual(m.status, 'evaluated');
  m.createCachedData();
  reject(new Error('boom'));
  await assert.rejects(evaluation, { message: 'boom' });
  assert.strictEqual(m.status, 'errored');
  assert.throws(() => m.createCachedData(), {
    code: 'ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA',
  });
})().then(common.mustCall());

// A code cache cannot be created while the module is evaluating.
(async () => {
  const context = createContext({ check: null });
  const m = new SourceTextModule('check();', { context });
  context.check = common.mustCall(() => {
    assert.strictEqual(m.status, 'evaluating');
    assert.throws(() => m.createCachedData(), {
      code: 'ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA',
    });
  });
  await m.link(common.mustNotCall());
  await m.evaluate();
})().then(common.mustCall());

// A code cache cannot be created after the evaluation failed.
(async () => {
  const m = new SourceTextModule('throw new Error("boom");');
  await m.link(common.mustNotCall());
  await assert.rejects(m.evaluate(), { message: 'boom' });
  assert.strictEqual(m.status, 'errored');
  assert.throws(() => m.createCachedData(), {
    code: 'ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA',
  });
})().then(common.mustCall());
