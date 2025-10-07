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
