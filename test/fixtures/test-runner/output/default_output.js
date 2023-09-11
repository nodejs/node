'use strict';
process.env.FORCE_COLOR = '1';
delete process.env.NODE_DISABLE_COLORS;
delete process.env.NO_COLOR;

require('../../../common');
const test = require('node:test');

test('should pass', () => {});
test('should fail', () => { throw new Error('fail'); });
test('should skip', { skip: true }, () => {});
test('parent', () => {
  test('should fail', () => { throw new Error('fail'); });
  test('should pass but parent fail', () => {});
});
