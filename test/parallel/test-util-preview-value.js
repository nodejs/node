'use strict';
require('../common');
const assert = require('assert');
const util = require('util');
const { previewValue } = util;

(async function() {
  // Primitives
  assert.strictEqual(
    await previewValue(-0),
    '-0'
  );
  assert.strictEqual(
    await previewValue(0n),
    '0n'
  );
  assert.strictEqual(
    await previewValue(undefined),
    'undefined'
  );
  assert.strictEqual(
    await previewValue(null),
    'null'
  );
  assert.strictEqual(
    await previewValue('foo'),
    "'foo'"
  );
  assert.strictEqual(
    await previewValue(Symbol('foo')),
    'Symbol(foo)'
  );

  // Private fields
  assert.strictEqual(
    await previewValue(new class Foo { #val = 'bar'; }()),
    "Foo { #val: 'bar' }"
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = 0; }()),
    'Foo { #val: 0 }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = -0; }()),
    'Foo { #val: -0 }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = 0n; }()),
    'Foo { #val: 0n }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = -0n; }()),
    'Foo { #val: 0n }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = Symbol('bar'); }()),
    'Foo { #val: Symbol(bar) }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = true; }()),
    'Foo { #val: true }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = undefined; }()),
    'Foo { #val: undefined }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = null; }()),
    'Foo { #val: null }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = () => {}; }()),
    'Foo { #val: [Function: #val] }'
  );
  assert.strictEqual(
    await previewValue(new class Foo { #val = {}; }()),
    'Foo { #val: #[Object] }'
  );

  // Functions
  assert.strictEqual(
    await previewValue(function foo() {}),
    '[Function: foo]'
  );

  // Nested objects
  {
    class Bar { val = 'public'; }
    assert.strictEqual(
      await previewValue(new class Foo { #val = new Bar(); }()),
      'Foo { #val: #[Bar] }'
    );
  }
})();
