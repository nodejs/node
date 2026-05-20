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
  assert.strictEqual(foo.hasTopLevelAwait(), false);
});

test('simple module with top-level await', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;

    await 0;
  `);
  assert.strictEqual(foo.hasTopLevelAwait(), true);
});

test('simple module with non top-level await', () => {
  const foo = new SourceTextModule(`
    export const foo = 4
    export default 5;

    export async function f() {
      await 0;
    }
  `);
  assert.strictEqual(foo.hasTopLevelAwait(), false);
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

  assert.strictEqual(foo.hasTopLevelAwait(), true);
  assert.strictEqual(bar.hasTopLevelAwait(), false);
});
