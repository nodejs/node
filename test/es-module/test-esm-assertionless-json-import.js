// Flags: --experimental-loader ./test/fixtures/es-module-loaders/assertionless-json-import.mjs
'use strict';
const common = require('../common');
const assert = require('assert');

async function test() {
  {
    const [secret0, secret1] = await Promise.all([
      import('../fixtures/experimental.json'),
      import(
        '../fixtures/experimental.json',
        { with: { type: 'json' } }
      ),
    ]);

    assert.strictEqual(secret0.default.ofLife, 42);
    assert.strictEqual(secret1.default.ofLife, 42);
    assert.strictEqual(secret0.default, secret1.default);
    assert.strictEqual(secret0, secret1);
  }

  {
    const [secret0, secret1] = await Promise.all([
      import('../fixtures/experimental.json?test'),
      import(
        '../fixtures/experimental.json?test',
        { with: { type: 'json' } }
      ),
    ]);

    assert.strictEqual(secret0.default.ofLife, 42);
    assert.strictEqual(secret1.default.ofLife, 42);
    assert.strictEqual(secret0.default, secret1.default);
    assert.strictEqual(secret0, secret1);
  }

  {
    const [secret0, secret1] = await Promise.all([
      import('../fixtures/experimental.json#test'),
      import(
        '../fixtures/experimental.json#test',
        { with: { type: 'json' } }
      ),
    ]);

    assert.strictEqual(secret0.default.ofLife, 42);
    assert.strictEqual(secret1.default.ofLife, 42);
    assert.strictEqual(secret0.default, secret1.default);
    assert.strictEqual(secret0, secret1);
  }

  {
    const [secret0, secret1] = await Promise.all([
      import('../fixtures/experimental.json?test2#test'),
      import(
        '../fixtures/experimental.json?test2#test',
        { with: { type: 'json' } }
      ),
    ]);

    assert.strictEqual(secret0.default.ofLife, 42);
    assert.strictEqual(secret1.default.ofLife, 42);
    assert.strictEqual(secret0.default, secret1.default);
    assert.strictEqual(secret0, secret1);
  }

  {
    const [secret0, secret1] = await Promise.all([
      import('data:application/json,{"ofLife":42}'),
      import(
        'data:application/json,{"ofLife":42}',
        { with: { type: 'json' } }
      ),
    ]);

    assert.strictEqual(secret0.default.ofLife, 42);
    assert.strictEqual(secret1.default.ofLife, 42);
  }
}

test().then(common.mustCall());
