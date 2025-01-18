import { describe, it, before, after, beforeEach, afterEach } from 'node:test';

describe('hooks order in bailout', () => {
  const order = [];

  before(() => {
    order.push('before');
  });

  after(() => {
    order.push('after');
    console.log('HOOKS_ORDER:', order.join(','));
  });

  beforeEach(() => {
    order.push('beforeEach');
  });

  afterEach(() => {
    order.push('afterEach');
  });

  it('first test that fails', () => {
    order.push('test1');
    throw new Error('First test failed');
  });

  it('second test that should not run', {
    before: () => order.push('before2'), // this should not run because of bailout
    after: () => order.push('after2'), // this should not run because of bailout
  }, () => {
    order.push('test2');
    throw new Error('This test should not run');
  });
});
