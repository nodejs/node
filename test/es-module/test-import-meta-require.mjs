import '../common/index.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';

describe('import.meta.require', () => {
  it('should require a built-in module', () => {
    const requiredAssert = import.meta.require('node:assert');
    assert.deepStrictEqual(assert, requiredAssert);
  });

  it('should require a esm module', () => {
    const { foo, bar } = import.meta.require('../fixtures/es-module-loaders/module-named-exports.mjs');
    assert.strictEqual(foo, 'foo');
    assert.strictEqual(bar, 'bar');
  });

  it('should require a cjs module', () => {
    const { foo, bar } = import.meta.require('../fixtures/es-modules/cjs-module-exports.js');
    assert.strictEqual(foo, 'foo');
    assert.strictEqual(bar, 'bar');
  });

  it('should require a json module', () => {
    const { foo, bar } = import.meta.require('../fixtures/es-modules/foobar.json');
    assert.strictEqual(foo, 'foo');
    assert.strictEqual(bar, 'bar');
  });

  it('should throw MODULE_NOT_FOUND', () => {
    assert.throws(() => import.meta.require('does-not-exist'), {
      code: 'MODULE_NOT_FOUND'
    });
  });
});
