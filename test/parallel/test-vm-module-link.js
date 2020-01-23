'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');
const { URL } = require('url');

const { SourceTextModule } = require('vm');

async function simple() {
  const foo = new SourceTextModule('export default 5;');
  await foo.link(common.mustNotCall());

  const bar = new SourceTextModule('import five from "foo"; five');

  assert.deepStrictEqual(bar.dependencySpecifiers, ['foo']);

  await bar.link(common.mustCall((specifier, module) => {
    assert.strictEqual(module, bar);
    assert.strictEqual(specifier, 'foo');
    return foo;
  }));

  assert.strictEqual((await bar.evaluate()).result, 5);
}

async function depth() {
  const foo = new SourceTextModule('export default 5');
  await foo.link(common.mustNotCall());

  async function getProxy(parentName, parentModule) {
    const mod = new SourceTextModule(`
      import ${parentName} from '${parentName}';
      export default ${parentName};
    `);
    await mod.link(common.mustCall((specifier, module) => {
      assert.strictEqual(module, mod);
      assert.strictEqual(specifier, parentName);
      return parentModule;
    }));
    return mod;
  }

  const bar = await getProxy('foo', foo);
  const baz = await getProxy('bar', bar);
  const barz = await getProxy('baz', baz);

  await barz.evaluate();

  assert.strictEqual(barz.namespace.default, 5);
}

async function circular() {
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
  await foo.link(common.mustCall(async (specifier, module) => {
    if (specifier === 'bar') {
      assert.strictEqual(module, foo);
      return bar;
    }
    assert.strictEqual(specifier, 'foo');
    assert.strictEqual(module, bar);
    assert.strictEqual(foo.status, 'linking');
    return foo;
  }, 2));

  assert.strictEqual(bar.status, 'linked');

  await foo.evaluate();
  assert.strictEqual(foo.namespace.default, 42);
}

async function circular2() {
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
  const rootModule = new SourceTextModule(sourceMap.root, {
    identifier: 'vm:root',
  });
  async function link(specifier, referencingModule) {
    if (moduleMap.has(specifier)) {
      return moduleMap.get(specifier);
    }
    const mod = new SourceTextModule(sourceMap[specifier], {
      identifier: new URL(specifier, 'file:///').href,
    });
    moduleMap.set(specifier, mod);
    return mod;
  }
  await rootModule.link(link);
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
