'use strict';

// Flags: --experimental-vm-modules

require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const test = require('node:test');

test('module is not instantiated yet', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;
  `);
  assert.throws(() => foo.hasAsyncGraph(), {
    code: 'ERR_VM_MODULE_STATUS',
  });
});

test('simple module with top-level await', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;

    await 0;
  `);
  foo.linkRequests([]);
  foo.instantiate();

  assert.strictEqual(foo.hasAsyncGraph(), true);
});

test('simple module with non top-level await', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;

    export async function f() {
      await 0;
    }
  `);
  foo.linkRequests([]);
  foo.instantiate();

  assert.strictEqual(foo.hasAsyncGraph(), false);
});

test('module with a dependency containing top-level await', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;

    await 0;
  `);
  foo.linkRequests([]);

  const bar = new SourceTextModule(`
    export { foo } from 'foo';
  `);
  bar.linkRequests([foo]);
  bar.instantiate();

  assert.strictEqual(foo.hasAsyncGraph(), true);
  assert.strictEqual(bar.hasAsyncGraph(), true);
});
