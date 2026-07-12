'use strict';
const { describe, it } = require('node:test');

describe('should fail', () => {
  throw new Error('error in describe');
});

describe('should pass', () => {
  it('ok', () => {});
});
