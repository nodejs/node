'use strict';
require('../common');
const { describe, it } = require('node:test');
const assert = require('assert');

describe('Concurrency option (boolean) = true ', { concurrency: true }, () => {
  let isFirstTestOver = false;
  it('should end after 1000ms', () => new Promise((resolve) => {
    setTimeout(() => { resolve(); isFirstTestOver = true; }, 1000);
  }));
  it('should start before the previous test ends', () => {
    assert.strictEqual(isFirstTestOver, false);
  });
});

describe(
  'Concurrency option (boolean) = false ',
  { concurrency: false },
  () => {
    let isFirstTestOver = false;
    it('should end after 1000ms', () => new Promise((resolve) => {
      setTimeout(() => { resolve(); isFirstTestOver = true; }, 1000);
    }));
    it('should start after the previous test ends', () => {
      assert.strictEqual(isFirstTestOver, true);
    });
  }
);
