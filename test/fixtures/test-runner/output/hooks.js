// Flags: --no-warnings
'use strict';
const common = require('../../../common');
const assert = require('assert');
const { describe, before, after, beforeEach, afterEach, test } = require('node:test');

describe('describe hooks', () => {
  before((t) => t.diagnostic('before 1 called'));

  before(function() {
    this.testArr = [];
    this.testArr.push('before ' + this.name);
  });

  beforeEach(function() {
    this.testArr.push('beforeEach ' + this.name);
  });

  afterEach(function() {
    this.testArr.push('afterEach ' + this.name);
  });

  after(function() {
    const expected = [
      'before describe hooks',
      'beforeEach 1', '1', 'afterEach 1',
      'beforeEach 2', '2', 'afterEach 2',
      'before nested',
      'beforeEach nested 1', '+beforeEach nested 1', 'nested 1', 'afterEach nested 1', '+afterEach nested 1',
      'beforeEach nested 2', '+beforeEach nested 2', 'nested 2', 'afterEach nested 2', '+afterEach nested 2',
      'after nested',
      'after describe hooks',
    ];
    assert.deepStrictEqual(this.testArr, expected);
  });

  test('1', function() {
    this.testArr.push('1');
  });

  test('2', function() {
    this.testArr.push('2');
  });

  describe('nested', () => {
    before(function() {
      this.testArr.push('before ' + this.name);
    });

    beforeEach(function() {
      this.testArr.push('+beforeEach ' + this.name);
    });

    afterEach(function() {
      this.testArr.push('+afterEach ' + this.name);
    });

    after(function() {
      this.testArr.push('after ' + this.name);
    });

    test('nested 1', function() {
      this.testArr.push('nested 1');
    });

    test('nested 2', function() {
      this.testArr.push('nested 2');
    });
  });
});

describe('before throws', () => {
  before(() => { throw new Error('before'); });
  test('1', () => {});
  test('2', () => {});
});

describe('after throws', () => {
  after(() => { throw new Error('after'); });
  test('1', () => {});
  test('2', () => {});
});

describe('beforeEach throws', () => {
  beforeEach(() => { throw new Error('beforeEach'); });
  test('1', () => {});
  test('2', () => {});
});

describe('afterEach throws', () => {
  afterEach(() => { throw new Error('afterEach'); });
  test('1', () => {});
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

  t.before((t) => testArr.push('before ' + t.name));
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
    'beforeEach 1', 'before test hooks', '1', 'afterEach 1',
    'beforeEach 2', '2', 'afterEach 2',
    'beforeEach nested',
    'beforeEach nested 1', 'nested beforeEach nested 1', 'nested1', 'afterEach nested 1', 'nested afterEach nested 1',
    'beforeEach nested 2', 'nested beforeEach nested 2', 'nested 2', 'afterEach nested 2', 'nested afterEach nested 2',
    'afterEach nested',
  ]);
});

test('t.before throws', async (t) => {
  t.after(common.mustCall());
  t.before(() => { throw new Error('before'); });
  await t.test('1', () => {});
  await t.test('2', () => {});
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

before((t) => t.diagnostic('before 2 called'));
