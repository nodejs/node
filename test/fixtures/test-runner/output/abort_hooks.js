'use strict';
const { before, beforeEach, describe, it, after, afterEach } = require('node:test');

describe('1 before describe', () => {
  const ac = new AbortController();
  before(() => {
    console.log('before');
    ac.abort();
  }, { signal: ac.signal });

  it('test 1', () => {
    console.log('1.1');
  });
  it('test 2', () => {
    console.log('1.2');
  });
});

describe('2 after describe', () => {
  const ac = new AbortController();
  after(() => {
    console.log('after');
    ac.abort();
  }, { signal: ac.signal });

  it('test 1', () => {
    console.log('2.1');
  });
  it('test 2', () => {
    console.log('2.2');
  });
});

describe('3 beforeEach describe', () => {
  const ac = new AbortController();
  beforeEach(() => {
    console.log('beforeEach');
    ac.abort();
  }, { signal: ac.signal });

  it('test 1', () => {
    console.log('3.1');
  });
  it('test 2', () => {
    console.log('3.2');
  });
});

describe('4 afterEach describe', () => {
  const ac = new AbortController();
  afterEach(() => {
    console.log('afterEach');
    ac.abort();
  }, { signal: ac.signal });

  it('test 1', () => {
    console.log('4.1');
  });
  it('test 2', () => {
    console.log('4.2');
  });
});
