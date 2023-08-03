// Flags: --test-only
'use strict';
require('../../../common');
const { test, describe, it } = require('node:test');

// These tests should be skipped based on the 'only' option.
test('only = undefined');
test('only = undefined, skip = string', { skip: 'skip message' });
test('only = undefined, skip = true', { skip: true });
test('only = undefined, skip = false', { skip: false });
test('only = false', { only: false });
test('only = false, skip = string', { only: false, skip: 'skip message' });
test('only = false, skip = true', { only: false, skip: true });
test('only = false, skip = false', { only: false, skip: false });

// These tests should be skipped based on the 'skip' option.
test('only = true, skip = string', { only: true, skip: 'skip message' });
test('only = true, skip = true', { only: true, skip: true });

// An 'only' test with subtests.
test('only = true, with subtests', { only: true }, async (t) => {
  // These subtests should run.
  await t.test('running subtest 1');
  await t.test('running subtest 2');

  // Switch the context to only execute 'only' tests.
  t.runOnly(true);
  await t.test('skipped subtest 1');
  await t.test('skipped subtest 2');
  await t.test('running subtest 3', { only: true });

  // Switch the context back to execute all tests.
  t.runOnly(false);
  await t.test('running subtest 4', async (t) => {
    // These subtests should run.
    await t.test('running sub-subtest 1');
    await t.test('running sub-subtest 2');

    // Switch the context to only execute 'only' tests.
    t.runOnly(true);
    await t.test('skipped sub-subtest 1');
    await t.test('skipped sub-subtest 2');
  });

  // Explicitly do not run these tests.
  await t.test('skipped subtest 3', { only: false });
  await t.test('skipped subtest 4', { skip: true });
});

describe.only('describe only = true, with subtests', () => {
  it.only('`it` subtest 1 should run', () => {});

  it('`it` subtest 2 should not run', async () => {});
});

describe.only('describe only = true, with a mixture of subtests', () => {
  it.only('`it` subtest 1', () => {});

  it.only('`it` async subtest 1', async () => {});

  it('`it` subtest 2 only=true', { only: true });

  it('`it` subtest 2 only=false', { only: false }, () => {
    throw new Error('This should not run');
  });

  it.skip('`it` subtest 3 skip', () => {
    throw new Error('This should not run');
  });

  it.todo('`it` subtest 4 todo', { only: false }, () => {
    throw new Error('This should not run');
  });

  test.only('`test` subtest 1', () => {});

  test.only('`test` async subtest 1', async () => {});

  test('`test` subtest 2 only=true', { only: true });

  test('`test` subtest 2 only=false', { only: false }, () => {
    throw new Error('This should not run');
  });

  test.skip('`test` subtest 3 skip', () => {
    throw new Error('This should not run');
  });

  test.todo('`test` subtest 4 todo', { only: false }, () => {
    throw new Error('This should not run');
  });
});

describe.only('describe only = true, with subtests', () => {
  test.only('subtest should run', () => {});

  test('async subtest should not run', async () => {});

  test('subtest should be skipped', { only: false }, () => {});
});
