'use strict';
const { describe, it } = require('node:test');
const { setTimeout } = require('node:timers/promises');

describe('planning with timeout', () => {
  it(`planning should pass if plan it's correct`, async (t) => {
    t.plan(1, { timeout: 500_000_000 });
    t.assert.ok(true);
  });
  
  it(`planning should fail if plan it's incorrect`, async (t) => {
    t.plan(1, { timeout: 500_000_000 });
    t.assert.ok(true);
    t.assert.ok(true);
  });
  
  it('planning with timeout', async (t) => {
    t.plan(1, { timeout: 2000 });
  
    while (true) {
      await setTimeout(5000);
    }
  });

  it('nested planning with timeout', async (t) => {
    t.plan(1, { timeout: 2000 });
  
    t.test('nested', async (t) => {
      while (true) {
        await setTimeout(5000);
      }
    });
  });
});
