// Flags: --no-warnings --test-name-pattern=enabled --test-name-pattern=/pattern/i
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

test('top level test disabled', common.mustNotCall());
test('top level skipped test disabled', { skip: true }, common.mustNotCall());
test('top level skipped test enabled', { skip: true }, common.mustNotCall());
it('top level it enabled', common.mustCall());
it('top level it disabled', common.mustNotCall());
it.skip('top level skipped it disabled', common.mustNotCall());
it.skip('top level skipped it enabled', common.mustNotCall());
describe('top level describe disabled', common.mustNotCall());
describe.skip('top level skipped describe disabled', common.mustNotCall());
describe.skip('top level skipped describe enabled', common.mustNotCall());
test('top level runs because name includes PaTtErN', common.mustCall());

test('top level test enabled', common.mustCall(async (t) => {
  t.beforeEach(common.mustCall());
  t.afterEach(common.mustCall());
  await t.test(
    'nested test runs because name includes PATTERN',
    common.mustCall(),
  );
}));

describe('top level describe enabled', () => {
  before(common.mustCall());
  beforeEach(common.mustCall(4));
  afterEach(common.mustCall(4));
  after(common.mustCall());

  it('nested it disabled', common.mustNotCall());
  it('nested it enabled', common.mustCall());
  describe('nested describe disabled', common.mustNotCall());
  describe('nested describe enabled', common.mustCall(() => {
    it('is enabled', common.mustCall());
  }));
});
