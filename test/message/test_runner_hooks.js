// Flags: --no-warnings
'use strict';
const common = require('../common');
const assert = require('assert');
const { test, describe, it, before, after, beforeEach, afterEach } = require('node:test');

describe('describe hooks', () => {
  const testArr = [];
  before(function() {
    testArr.push('before ' + this.name);
  });
  after(function() {
    testArr.push('after ' + this.name);
    assert.deepStrictEqual(testArr, [
      'before describe hooks',
      'beforeEach 1', '1', 'afterEach 1',
      'beforeEach 2', '2', 'afterEach 2',
      'beforeEach nested',
      'before nested',
      'beforeEach nested 1', 'nested 1', 'afterEach nested 1',
      'beforeEach nested 2', 'nested 2', 'afterEach nested 2',
      'after nested',
      'afterEach nested',
      'after describe hooks',
    ]);
  });
  beforeEach(function() {
    testArr.push('beforeEach ' + this.name);
  });
  afterEach(function() {
    testArr.push('afterEach ' + this.name);
  });

  it('1', () => testArr.push('1'));
  test('2', () => testArr.push('2'));

  describe('nested', () => {
    before(function() {
      testArr.push('before ' + this.name);
    });
    after(function() {
      testArr.push('after ' + this.name);
    });
    beforeEach(function() {
      testArr.push('beforeEach ' + this.name);
    });
    afterEach(function() {
      testArr.push('afterEach ' + this.name);
    });
    it('nested 1', () => testArr.push('nested 1'));
    test('nested 2', () => testArr.push('nested 2'));
  });
});

describe('before throws', () => {
  before(() => { throw new Error('before'); });
  it('1', () => {});
  test('2', () => {});
});

describe('after throws', () => {
  after(() => { throw new Error('after'); });
  it('1', () => {});
  test('2', () => {});
});

describe('beforeEach throws', () => {
  beforeEach(() => { throw new Error('beforeEach'); });
  it('1', () => {});
  test('2', () => {});
});

describe('afterEach throws', () => {
  afterEach(() => { throw new Error('afterEach'); });
  it('1', () => {});
  test('2', () => {});
});

describe('afterEach when test fails', () => {
  afterEach(common.mustCall(2));
  it('1', () => { throw new Error('test'); });
  test('2', () => {});
});

describe('afterEach throws and test fails', () => {
  afterEach(() => { throw new Error('afterEach'); });
  it('1', () => { throw new Error('test'); });
  test('2', () => {});
});

test('test hooks', async (t) => {
  const testArr = [];

  t.after(common.mustCall((t) => testArr.push('after ' + t.name)));
  t.beforeEach((t) => testArr.push('beforeEach ' + t.name));
  t.afterEach((t) => testArr.push('afterEach ' + t.name));
  await t.test('1', () => testArr.push('1'));
  await t.test('2', () => testArr.push('2'));

  await t.test('nested', async (t) => {
    t.beforeEach((t) => testArr.push('nested beforeEach ' + t.name));
    t.afterEach((t) => testArr.push('nested afterEach ' + t.name));
    await t.test('nested 1', () => testArr.push('nested1'));
    await t.test('nested 2', () => testArr.push('nested 2'));
  });

  assert.deepStrictEqual(testArr, [
    'beforeEach 1', '1', 'afterEach 1',
    'beforeEach 2', '2', 'afterEach 2',
    'beforeEach nested',
    'nested beforeEach nested 1', 'nested1', 'nested afterEach nested 1',
    'nested beforeEach nested 2', 'nested 2', 'nested afterEach nested 2',
    'afterEach nested',
  ]);
});

test('t.beforeEach throws', async (t) => {
  t.after(common.mustCall());
  t.beforeEach(() => { throw new Error('beforeEach'); });
  await t.test('1', () => {});
  await t.test('2', () => {});
});

test('t.afterEach throws', async (t) => {
  t.after(common.mustCall());
  t.afterEach(() => { throw new Error('afterEach'); });
  await t.test('1', () => {});
  await t.test('2', () => {});
});


test('afterEach when test fails', async (t) => {
  t.after(common.mustCall());
  t.afterEach(common.mustCall(2));
  await t.test('1', () => { throw new Error('test'); });
  await t.test('2', () => {});
});

test('afterEach throws and test fails', async (t) => {
  t.after(common.mustCall());
  t.afterEach(() => { throw new Error('afterEach'); });
  await t.test('1', () => { throw new Error('test'); });
  await t.test('2', () => {});
});

test('t.after() is called if test body throws', (t) => {
  t.after(() => {
    t.diagnostic('- after() called');
  });
  throw new Error('bye');
});
