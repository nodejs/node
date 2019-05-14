'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
const assert = require('assert');
const { SourceTextModule, createContext } = require('vm');

(async function test1() {
  const context = createContext({
    foo: 'bar',
    baz: undefined,
    typeofProcess: undefined,
  });
  const m = new SourceTextModule(
    'baz = foo; typeofProcess = typeof process; typeof Object;',
    { context }
  );
  assert.strictEqual(m.status, 'uninstantiated');
  await m.link(common.mustNotCall());
  m.instantiate();
  assert.strictEqual(m.status, 'instantiated');
  const result = await m.evaluate();
  assert.strictEqual(m.status, 'evaluated');
  assert.strictEqual(Object.getPrototypeOf(result), null);
  assert.deepStrictEqual(context, {
    foo: 'bar',
    baz: 'bar',
    typeofProcess: 'undefined'
  });
  assert.strictEqual(result.result, 'function');
}());

(async () => {
  const m = new SourceTextModule(
    'global.vmResult = "foo"; Object.prototype.toString.call(process);'
  );
  await m.link(common.mustNotCall());
  m.instantiate();
  const { result } = await m.evaluate();
  assert.strictEqual(global.vmResult, 'foo');
  assert.strictEqual(result, '[object process]');
  delete global.vmResult;
})();

(async () => {
  const m = new SourceTextModule('while (true) {}');
  await m.link(common.mustNotCall());
  m.instantiate();
  await m.evaluate({ timeout: 500 })
    .then(() => assert(false), () => {});
})();

// Check the generated url for each module
(async () => {
  const context1 = createContext({ });
  const context2 = createContext({ });

  const m1 = new SourceTextModule('1', { context: context1 });
  assert.strictEqual(m1.url, 'vm:module(0)');
  const m2 = new SourceTextModule('2', { context: context1 });
  assert.strictEqual(m2.url, 'vm:module(1)');
  const m3 = new SourceTextModule('3', { context: context2 });
  assert.strictEqual(m3.url, 'vm:module(0)');
})();
