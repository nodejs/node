'use strict';
const common = require('../common');
const assert = require('assert');

async function test() {
  {
    const results = await Promise.allSettled([
      import('../fixtures/empty.js', { with: { type: 'json' } }),
      import('../fixtures/empty.js'),
    ]);

    assert.strictEqual(results[0].status, 'rejected');
    assert.strictEqual(results[1].status, 'fulfilled');
  }

  {
    const results = await Promise.allSettled([
      import('../fixtures/empty.js'),
      import('../fixtures/empty.js', { with: { type: 'json' } }),
    ]);

    assert.strictEqual(results[0].status, 'fulfilled');
    assert.strictEqual(results[1].status, 'rejected');
  }

  {
    const results = await Promise.allSettled([
      import('../fixtures/empty.json', { with: { type: 'json' } }),
      import('../fixtures/empty.json'),
    ]);

    assert.strictEqual(results[0].status, 'fulfilled');
    assert.strictEqual(results[1].status, 'rejected');
  }

  {
    const results = await Promise.allSettled([
      import('../fixtures/empty.json'),
      import('../fixtures/empty.json', { with: { type: 'json' } }),
    ]);

    assert.strictEqual(results[0].status, 'rejected');
    assert.strictEqual(results[1].status, 'fulfilled');
  }
}

test().then(common.mustCall());
