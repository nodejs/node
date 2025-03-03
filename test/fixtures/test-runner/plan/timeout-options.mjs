import test from 'node:test';
import * as common from '../../../common/index.mjs';

test('with wait:true should PASS', (t) => {
  t.plan(1, { wait: true });
  setTimeout(() => {
    t.assert.ok(true);
  }, common.platformTimeout(500));
});

test('with wait:false should NOT wait for assertions', (t) => {
  t.plan(1, { wait: false });
  setTimeout(() => {
    t.assert.ok(true);
  }, common.platformTimeout(30_000));
});
