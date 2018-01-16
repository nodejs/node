'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');
const { URL } = require('url');

const { Module } = require('vm');

async function simple() {
  const foo = new Module('export default 5;');
  await foo.link(common.mustNotCall());

  const bar = new Module('import five from "foo"; five');

  assert.deepStrictEqual(bar.dependencySpecifiers, ['foo']);

  await bar.link(common.mustCall((module, specifier) => {
    assert.strictEqual(module, bar);
    assert.strictEqual(specifier, 'foo');
    return foo;
  }));

  bar.instantiate();

  assert.strictEqual((await bar.evaluate()).result, 5);
}

async function depth() {
  const foo = new Module('export default 5');
  await foo.link(common.mustNotCall());

  async function getProxy(parentName, parentModule) {
    const mod = new Module(`
      import ${parentName} from '${parentName}';
      export default ${parentName};
    `);
    await mod.link(common.mustCall((module, specifier) => {
      assert.strictEqual(module, mod);
      assert.strictEqual(specifier, parentName);
      return parentModule;
    }));
    return mod;
  }

  const bar = await getProxy('foo', foo);
  const baz = await getProxy('bar', bar);
  const barz = await getProxy('baz', baz);

  barz.instantiate();
  await barz.evaluate();

  assert.strictEqual(barz.namespace.default, 5);
}

async function circular() {
  const foo = new Module(`
    import getFoo from 'bar';
    export let foo = 42;
    export default getFoo();
  `);
  const bar = new Module(`
    import { foo } from 'foo';
    export default function getFoo() {
      return foo;
    }
  `);
  await foo.link(common.mustCall(async (fooModule, fooSpecifier) => {
    assert.strictEqual(fooModule, foo);
    assert.strictEqual(fooSpecifier, 'bar');
    await bar.link(common.mustCall((barModule, barSpecifier) => {
      assert.strictEqual(barModule, bar);
      assert.strictEqual(barSpecifier, 'foo');
      assert.strictEqual(foo.linkingStatus, 'linking');
      return foo;
    }));
    assert.strictEqual(bar.linkingStatus, 'linked');
    return bar;
  }));

  foo.instantiate();
  await foo.evaluate();
  assert.strictEqual(foo.namespace.default, 42);
}

async function circular2() {
  const sourceMap = {
    root: `
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
      export var fromA;
    `,
    './b.mjs': `
      export * from './a.mjs';
      export var fromB;
    `
  };
  const moduleMap = new Map();
  const rootModule = new Module(sourceMap.root, { url: 'vm:root' });
  async function link(referencingModule, specifier) {
    if (moduleMap.has(specifier)) {
      return moduleMap.get(specifier);
    }
    const mod = new Module(sourceMap[specifier], { url: new URL(specifier, 'file:///').href });
    moduleMap.set(specifier, mod);
    return mod;
  }
  await rootModule.link(link);
  rootModule.instantiate();
  await rootModule.evaluate();
}

const finished = common.mustCall();

(async function main() {
  await simple();
  await depth();
  await circular();
  await circular2();
  finished();
})();
