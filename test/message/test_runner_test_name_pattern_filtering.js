// Flags: --no-warnings --test-name-pattern=suite-one>suite-two>subtest-one
'use strict';
const common = require('../common');
const {
  after,
  afterEach,
  before,
  beforeEach,
  describe,
  it,
  test,
} = require('node:test');

describe('suite-one', () => {
  before(common.mustCall());
  beforeEach(common.mustCall(3));
  afterEach(common.mustCall(3));
  after(common.mustCall());

  it('subtest-one', common.mustNotCall());
  describe(
    'suite-two',
    common.mustCall(() => {
      it('subtest-one', common.mustCall());
    }),
  );
  describe('suite-three', common.mustNotCall());
});

test(
  'suite-one',
  common.mustCall(async (t) => {
    t.beforeEach(common.mustCall(3));
    t.afterEach(common.mustCall(3));

    await t.test('subtest-one', common.mustNotCall());
    await t.test(
      'suite-two',
      common.mustCall(async (t) => {
        await t.test('subtest-one', common.mustCall());
      }),
    );
    await t.test('suite-three', common.mustNotCall());
  }),
);

describe('excluded', () => {
  before(common.mustNotCall());
  beforeEach(common.mustNotCall());
  afterEach(common.mustNotCall());
  after(common.mustNotCall());

  it('subtest-one', common.mustNotCall());
  describe('suite-two', common.mustNotCall());
});

test('excluded', (t) => {
  t.beforeEach(common.mustNotCall());
  t.afterEach(common.mustNotCall());

  t.test('subtest-one', common.mustNotCall());
  t.test('suite-two', common.mustNotCall());
});
