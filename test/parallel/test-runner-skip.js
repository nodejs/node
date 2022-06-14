'use strict';
require('../common');
const test = require('node:test');
const assert = require('assert');


const tracker = new assert.CallTracker();
const func = tracker.calls(() => {}, 1);

test('test skip recipes', async (t) => {
  await t.skip('skip using test.skip', func);
  await t.test('skip using options', { skip: true }, func);
  await t.test('skip internally using context', (t) => {
    t.skip();
    func();
  });

  tracker.verify();
});
