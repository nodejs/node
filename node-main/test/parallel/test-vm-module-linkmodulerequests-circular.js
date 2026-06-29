'use strict';

// Flags: --experimental-vm-modules --js-source-phase-imports

require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const test = require('node:test');

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
