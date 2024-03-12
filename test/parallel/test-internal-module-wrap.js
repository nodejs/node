// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

const foo = new ModuleWrap('foo', undefined, 'export * from "bar";', 0, 0);
const bar = new ModuleWrap('bar', undefined, 'export const five = 5', 0, 0);

(async () => {
  assert.strictEqual(foo.moduleRequests.length, 1);
  assert.strictEqual(foo.moduleRequests[0].specifier, 'bar');

  foo.link(['bar'], [bar]);
  foo.instantiate();

  assert.strictEqual(await foo.evaluate(-1, false), undefined);
  assert.strictEqual(foo.getNamespace().five, 5);
})().then(common.mustCall());
