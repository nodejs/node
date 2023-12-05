'use strict';
require('../../../common');
const { describe, it } = require('node:test');

describe('nested - no tests', () => {
  describe('nested', () => {
    it('nested', () => {});
  });
});
