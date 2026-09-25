'use strict';

const common = require('../common');
const assert = require('node:assert');
const { beforeEach, afterEach, test } = require('node:test');

let beforeEachTotal = 0;
let afterEachRuntimeSkip = 0;
let afterEachTotal = 0;

beforeEach(common.mustCall(() => {
  beforeEachTotal++;
}, 2));

afterEach(common.mustCall((t) => {
  afterEachTotal++;
  if (t.name === 'runtime skip') {
    afterEachRuntimeSkip++;
  }
}, 2));

test('normal test', (t) => {
  t.assert.ok(true);
});

test('runtime skip', (t) => {
  t.skip('skip after setup');
});

test('static skip', { skip: true }, common.mustNotCall());

process.on('exit', () => {
  assert.strictEqual(beforeEachTotal, 2);
  assert.strictEqual(afterEachRuntimeSkip, 1);
  assert.strictEqual(afterEachTotal, 2);
});
