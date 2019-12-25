'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
const assert = require('assert');
const { SourceTextModule } = require('vm');

async function testBasic() {
  const m = new SourceTextModule('import.meta;', {
    initializeImportMeta: common.mustCall((meta, module) => {
      assert.strictEqual(module, m);
      meta.prop = 42;
    })
  });
  await m.link(common.mustNotCall());
  const { result } = await m.evaluate();
  assert.strictEqual(typeof result, 'object');
  assert.strictEqual(Object.getPrototypeOf(result), null);
  assert.strictEqual(result.prop, 42);
  assert.deepStrictEqual(Reflect.ownKeys(result), ['prop']);
}

async function testInvalid() {
  for (const invalidValue of [
    null, {}, 0, Symbol.iterator, [], 'string', false
  ]) {
    assert.throws(() => {
      new SourceTextModule('', {
        initializeImportMeta: invalidValue
      });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
  }
}

(async () => {
  await testBasic();
  await testInvalid();
})();
