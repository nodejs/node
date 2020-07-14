// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');
const { getPromiseDetails, isPromise } = internalBinding('util');
const setTimeoutAsync = require('util').promisify(setTimeout);

const foo = new ModuleWrap('foo', undefined, 'export * from "bar";', 0, 0);
const bar = new ModuleWrap('bar', undefined, 'export const five = 5', 0, 0);

(async () => {
  const promises = foo.link(() => setTimeoutAsync(1000).then(() => bar));
  assert.strictEqual(promises.length, 1);
  assert(isPromise(promises[0]));

  await Promise.all(promises);

  assert.strictEqual(getPromiseDetails(promises[0])[1], bar);

  foo.instantiate();

  assert.strictEqual(await foo.evaluate(-1, false), undefined);
  assert.strictEqual(foo.getNamespace().five, 5);
})().then(common.mustCall());
