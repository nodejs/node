'use strict';
require('../common');

// Return type of shorthands should be consistent
// with the return type of test

const assert = require('assert');
const { test, describe, it } = require('node:test');
const { isPromise } = require('util/types');

const testOnly = test('only test', { only: true });
const testTodo = test('todo test', { todo: true });
const testSkip = test('skip test', { skip: true });
const testOnlyShorthand = test.only('only test shorthand');
const testTodoShorthand = test.todo('todo test shorthand');
const testSkipShorthand = test.skip('skip test shorthand');

describe('\'node:test\' and its shorthands should return the same', () => {
  it('should return a Promise', () => {
    assert(isPromise(testOnly));
    assert(isPromise(testTodo));
    assert(isPromise(testSkip));
    assert(isPromise(testOnlyShorthand));
    assert(isPromise(testTodoShorthand));
    assert(isPromise(testSkipShorthand));
  });

  it('should resolve undefined', async () => {
    assert.strictEqual(await testOnly, undefined);
    assert.strictEqual(await testTodo, undefined);
    assert.strictEqual(await testSkip, undefined);
    assert.strictEqual(await testOnlyShorthand, undefined);
    assert.strictEqual(await testTodoShorthand, undefined);
    assert.strictEqual(await testSkipShorthand, undefined);
  });
});
