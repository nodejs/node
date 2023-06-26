'use strict';
const common = require('../common');

// Return type of shorthands should be consistent
// with the return type of test

const assert = require('assert');
const test = require('node:test');
const { isPromise } = require('util/types');

const testOnly = test({ only: true });
const testTodo = test({ todo: true });
const testSkip = test({ skip: true });
const testOnlyShorthand = test.only();
const testTodoShorthand = test.todo();
const testSkipShorthand = test.skip();

// return Promise
assert(isPromise(testOnly));
assert(isPromise(testTodo));
assert(isPromise(testSkip));
assert(isPromise(testOnlyShorthand));
assert(isPromise(testTodoShorthand));
assert(isPromise(testSkipShorthand));

// resolve to undefined
(async () => {
  assert.strictEqual(await testOnly, undefined);
  assert.strictEqual(await testTodo, undefined);
  assert.strictEqual(await testSkip, undefined);
  assert.strictEqual(await testOnlyShorthand, undefined);
  assert.strictEqual(await testTodoShorthand, undefined);
  assert.strictEqual(await testSkipShorthand, undefined);
})().then(common.mustCall());
