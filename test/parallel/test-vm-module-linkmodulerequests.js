'use strict';

// Flags: --experimental-vm-modules --js-source-phase-imports

require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const test = require('node:test');

test('simple graph', async function simple() {
  const foo = new SourceTextModule('export default 5;');
  foo.linkRequests([]);

  globalThis.fiveResult = undefined;
  const bar = new SourceTextModule('import five from "foo"; fiveResult = five');

  bar.linkRequests([foo]);
  bar.instantiate();

  await bar.evaluate();
  assert.strictEqual(globalThis.fiveResult, 5);
  delete globalThis.fiveResult;
});

test('invalid link values', () => {
  const invalidValues = [
    undefined,
    null,
    {},
    SourceTextModule.prototype,
  ];

  for (const value of invalidValues) {
    const module = new SourceTextModule('import "foo"');
    assert.throws(() => module.linkRequests([value]), {
      code: 'ERR_VM_MODULE_NOT_MODULE',
    });
  }
});

test('mismatch linkage', () => {
  const foo = new SourceTextModule('export default 5;');
  foo.linkRequests([]);

  // Link with more modules than requested.
  const bar = new SourceTextModule('import foo from "foo";');
  assert.throws(() => bar.linkRequests([foo, foo]), {
    code: 'ERR_MODULE_LINK_MISMATCH',
  });

  // Link with fewer modules than requested.
  const baz = new SourceTextModule('import foo from "foo"; import bar from "bar";');
  assert.throws(() => baz.linkRequests([foo]), {
    code: 'ERR_MODULE_LINK_MISMATCH',
  });

  // Link a same module cache key with different instances.
  const qux = new SourceTextModule(`
    import foo from "foo";
    import source Foo from "foo";
  `);
  assert.throws(() => qux.linkRequests([foo, bar]), {
    code: 'ERR_MODULE_LINK_MISMATCH',
  });
});

test('deep linking', async function depth() {
  const foo = new SourceTextModule('export default 5');
  foo.linkRequests([]);
  foo.instantiate();

  function getProxy(parentName, parentModule) {
    const mod = new SourceTextModule(`
      import ${parentName} from '${parentName}';
      export default ${parentName};
    `);
    mod.linkRequests([parentModule]);
    mod.instantiate();
    return mod;
  }

  const bar = getProxy('foo', foo);
  const baz = getProxy('bar', bar);
  const barz = getProxy('baz', baz);

  await barz.evaluate();

  assert.strictEqual(barz.namespace.default, 5);
});

test('basic circular linking', async function circular() {
  const foo = new SourceTextModule(`
    import getFoo from 'bar';
    export let foo = 42;
    export default getFoo();
  `);
  const bar = new SourceTextModule(`
    import { foo } from 'foo';
    export default function getFoo() {
      return foo;
    }
  `);
  foo.linkRequests([bar]);
  bar.linkRequests([foo]);

  foo.instantiate();
  assert.strictEqual(foo.status, 'linked');
  assert.strictEqual(bar.status, 'linked');

  await foo.evaluate();
  assert.strictEqual(foo.namespace.default, 42);
});

test('circular linking graph', async function circular2() {
  const sourceMap = {
    'root': `
      import * as a from './a.mjs';
      import * as b from './b.mjs';
      if (!('fromA' in a))
        throw new Error();
      if (!('fromB' in a))
        throw new Error();
      if (!('fromA' in b))
        throw new Error();
      if (!('fromB' in b))
        throw new Error();
    `,
    './a.mjs': `
      export * from './b.mjs';
      export let fromA;
    `,
    './b.mjs': `
      export * from './a.mjs';
      export let fromB;
    `
  };
  const moduleMap = new Map();
  for (const [specifier, source] of Object.entries(sourceMap)) {
    moduleMap.set(specifier, new SourceTextModule(source, {
      identifier: new URL(specifier, 'file:///').href,
    }));
  }
  for (const mod of moduleMap.values()) {
    mod.linkRequests(mod.moduleRequests.map((request) => {
      return moduleMap.get(request.specifier);
    }));
  }
  const rootModule = moduleMap.get('root');
  rootModule.instantiate();
  await rootModule.evaluate();
});
