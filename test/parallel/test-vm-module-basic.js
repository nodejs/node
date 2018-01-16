'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
const assert = require('assert');
const { Module, createContext } = require('vm');

common.crashOnUnhandledRejection();

(async function test1() {
  const context = createContext({
    foo: 'bar',
    baz: undefined,
    typeofProcess: undefined,
  });
  const m = new Module(
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
  const m = new Module(
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
  const m = new Module('while (true) {}');
  await m.link(common.mustNotCall());
  m.instantiate();
  await m.evaluate({ timeout: 500 })
    .then(() => assert(false), () => {});
})();
