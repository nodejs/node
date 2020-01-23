'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
const assert = require('assert');
const {
  Module,
  SourceTextModule,
  SyntheticModule,
  createContext
} = require('vm');
const util = require('util');

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
  assert.strictEqual(m.status, 'unlinked');
  await m.link(common.mustNotCall());
  assert.strictEqual(m.status, 'linked');
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
  const { result } = await m.evaluate();
  assert.strictEqual(global.vmResult, 'foo');
  assert.strictEqual(result, '[object process]');
  delete global.vmResult;
})();

(async () => {
  const m = new SourceTextModule('while (true) {}');
  await m.link(common.mustNotCall());
  await m.evaluate({ timeout: 500 })
    .then(() => assert(false), () => {});
})();

// Check the generated identifier for each module
(async () => {
  const context1 = createContext({ });
  const context2 = createContext({ });

  const m1 = new SourceTextModule('1', { context: context1 });
  assert.strictEqual(m1.identifier, 'vm:module(0)');
  const m2 = new SourceTextModule('2', { context: context1 });
  assert.strictEqual(m2.identifier, 'vm:module(1)');
  const m3 = new SourceTextModule('3', { context: context2 });
  assert.strictEqual(m3.identifier, 'vm:module(0)');
})();

// Check inspection of the instance
{
  const context = createContext({ foo: 'bar' });
  const m = new SourceTextModule('1', { context });

  assert.strictEqual(
    util.inspect(m),
    `SourceTextModule {
  status: 'unlinked',
  identifier: 'vm:module(0)',
  context: { foo: 'bar' }
}`
  );

  assert.strictEqual(util.inspect(m, { depth: -1 }), '[SourceTextModule]');

  assert.strictEqual(
    m[util.inspect.custom].call(Object.create(null)),
    'Module { status: undefined, identifier: undefined, context: undefined }',
  );
}

{
  const context = createContext({ foo: 'bar' });
  const m = new SyntheticModule([], () => {}, { context });

  assert.strictEqual(
    util.inspect(m),
    `SyntheticModule {
  status: 'unlinked',
  identifier: 'vm:module(0)',
  context: { foo: 'bar' }
}`
  );

  assert.strictEqual(util.inspect(m, { depth: -1 }), '[SyntheticModule]');
}

// Check dependencies getter returns same object every time
{
  const m = new SourceTextModule('');
  const dep = m.dependencySpecifiers;
  assert.notStrictEqual(dep, undefined);
  assert.strictEqual(dep, m.dependencySpecifiers);
}

// Check the impossibility of creating an abstract instance of the Module.
{
  assert.throws(() => new Module(), {
    message: 'Module is not a constructor',
    name: 'TypeError'
  });
}

// Check to throws invalid exportNames
{
  assert.throws(() => new SyntheticModule(undefined, () => {}, {}), {
    message: 'The "exportNames" argument must be an Array of strings.' +
      ' Received undefined',
    name: 'TypeError'
  });
}

// Check to throws invalid evaluateCallback
{
  assert.throws(() => new SyntheticModule([], undefined, {}), {
    message: 'The "evaluateCallback" argument must be of type function.' +
      ' Received undefined',
    name: 'TypeError'
  });
}

// Check to throws invalid options
{
  assert.throws(() => new SyntheticModule([], () => {}, null), {
    message: 'The "options" argument must be of type object.' +
      ' Received null',
    name: 'TypeError'
  });
}
