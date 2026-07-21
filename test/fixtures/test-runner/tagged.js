'use strict';
const { describe, it, test } = require('node:test');

describe('db suite', { tags: ['db'] }, () => {
  it('db only', () => {});
  it('db plus integration', { tags: ['integration'] }, () => {});
  it('db flaky', { tags: ['flaky'] }, () => {});
});

describe('unit suite', { tags: ['unit'] }, () => {
  it('unit only', () => {});
  it('unit slow', { tags: ['slow'] }, () => {});
});

test('untagged', () => {});
test('only flaky', { tags: ['flaky'] }, () => {});
test('db wildcard match', { tags: ['db:postgres'] }, () => {});

describe('plain suite', () => {
  it('lonely child', { tags: ['lonely'] }, () => {});
  it('plain sibling', () => {});
});
