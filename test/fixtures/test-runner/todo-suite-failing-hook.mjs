import { before, describe, it } from 'node:test';

describe('todo suite with failing before hook', { todo: 'evaluating' }, () => {
  before(() => {
    throw new Error('simulated cleanup failure');
  });

  it('child 1', () => {});
  it('child 2', () => {});
});
