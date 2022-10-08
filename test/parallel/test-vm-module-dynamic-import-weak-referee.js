'use strict';

// Flags: --experimental-vm-modules --expose-gc

const common = require('../common');

const assert = require('assert');
const { Script, SourceTextModule } = require('vm');

test();
async function test() {
  const foo = new SourceTextModule('export const a = 1;');
  await foo.link(common.mustNotCall());
  await foo.evaluate();

  {
    const s = new Script(`
      globalThis.result = new Promise(resolve => {
        setTimeout(() => {
          gc(); // gc the vm.Script instance, causing executing 'import' in microtask a segment fault
          resolve(import("foo"));
        }, 1);
      });
    `, {
      importModuleDynamically: common.mustCall((specifier, wrap) => {
        assert.strictEqual(specifier, 'foo');
        assert.strictEqual(wrap, s);
        return foo;
      }),
    });

    s.runInThisContext();
    assert.ok(globalThis.result);
  }
  global.gc();
  assert.strictEqual(foo.namespace, await globalThis.result);
  delete global.result;
}
