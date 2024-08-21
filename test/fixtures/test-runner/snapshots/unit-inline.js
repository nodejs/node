'use strict';
const { snapshot, suite, test } = require('node:test');
const { basename, join } = require('node:path');

snapshot.setResolveSnapshotPath((testFile) => {
  return join(process.cwd(), `${basename(testFile)}.snapshot`);
});

suite('suite', () => {
  test('test with plan', (t) => {
    t.plan(2);
    t.assert.inlineSnapshot({ foo: 1, bar: 2 }, `{
  "foo": 1,
  "bar": 2
}`);
    t.assert.inlineSnapshot(5, `5`);
  });
});

test('test', async (t) => {
  t.assert.inlineSnapshot({ baz: 9 }, `{
  "baz": 9
}`);
});

test('number', async (t) => {
  t.assert.inlineSnapshot(42, `42`);
});

test('`${foo}`', async (t) => {
  const options = { serializers: [() => { return '***'; }]};
  t.assert.inlineSnapshot('snapshotted string', `***`, options);
});

test('no escape `\\${foo}`\n except for default serializers', async (t) => {
  const input = '`\\${foo}`\n';
  t.assert.inlineSnapshot(input, JSON.stringify(input, null, 2));
});

test('no change `\\${foo}`\n with empty serializers override', async (t) => {
  const input = '`\\${foo}`\n';
  t.assert.inlineSnapshot(input, input, { serializers: []});
});

require('./imported-tests-inline');
