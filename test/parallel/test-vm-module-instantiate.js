'use strict';

// Flags: --experimental-vm-modules

require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const test = require('node:test');

test('simple module', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;
  `);
  foo.linkRequestedModules([]);
  foo.instantiate();

  assert.deepStrictEqual(
    Reflect.ownKeys(foo.namespace),
    ['default', 'foo', Symbol.toStringTag]
  );
});

test('linkRequestedModules can be skipped', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;
  `);
  foo.instantiate();

  assert.deepStrictEqual(
    Reflect.ownKeys(foo.namespace),
    ['default', 'foo', Symbol.toStringTag]
  );
});

test('re-export simple name', () => {
  const foo = new SourceTextModule(`
    export { bar } from 'bar';
  `);
  const bar = new SourceTextModule(`
    export const bar = 42;
  `);
  foo.linkRequestedModules([bar]);
  foo.instantiate();

  assert.deepStrictEqual(
    Reflect.ownKeys(foo.namespace),
    ['bar', Symbol.toStringTag]
  );
});

test('re-export-star', () => {
  const foo = new SourceTextModule(`
    export * from 'bar';
  `);
  const bar = new SourceTextModule(`
    export const bar = 42;
  `);
  foo.linkRequestedModules([bar]);
  foo.instantiate();

  assert.deepStrictEqual(
    Reflect.ownKeys(foo.namespace),
    ['bar', Symbol.toStringTag]
  );
});

test('deep re-export-star', () => {
  let stackTop = new SourceTextModule(`
    export const foo = 4;
  `);
  stackTop.linkRequestedModules([]);
  for (let i = 0; i < 10; i++) {
    const mod = new SourceTextModule(`
      export * from 'stack?top';
    `);
    mod.linkRequestedModules([stackTop]);
    stackTop = mod;
  }
  stackTop.instantiate();

  assert.deepStrictEqual(
    Reflect.ownKeys(stackTop.namespace),
    ['foo', Symbol.toStringTag]
  );
});

test('should throw if the module is not linked', () => {
  const foo = new SourceTextModule(`
    import { bar } from 'bar';
  `);
  assert.throws(() => {
    foo.instantiate();
  }, {
    code: 'ERR_VM_MODULE_LINK_FAILURE',
  });
});
