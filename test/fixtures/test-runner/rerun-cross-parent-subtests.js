'use strict';

const { describe, it } = require('node:test');
const assert = require('node:assert');

function makeSuite(shouldPass, label) {
  return async (t) => {
    await t.test('inner', async () => {
      if (!shouldPass) assert.fail(`${label} should fail`);
    });
  };
}

describe('parents', { concurrency: false }, () => {
  it('A passes', makeSuite(true, 'A'));
  it('B fails', makeSuite(false, 'B'));
  it('C passes', makeSuite(true, 'C'));
});
