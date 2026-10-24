// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { AsyncResource, executionAsyncResource } = require('async_hooks');
const { internalBinding } = require('internal/test/binding');
const { FSReqCallback } = internalBinding('fs');
const { async_hook_fields, constants: { kLazyScopes } } = internalBinding('async_wrap');

// No hook and no executionAsyncResource() yet, so the scope of this callback skips the id stack.
// Asked for here, its resource comes off the chain of lazy scopes, and for an AsyncWrap the
// resource is held as a Global.
fs.stat(__filename, common.mustSucceed(() => {
  assert.ok(async_hook_fields[kLazyScopes] > 0);
  const resource = executionAsyncResource();
  assert.ok(resource instanceof FSReqCallback);
  assert.strictEqual(executionAsyncResource(), resource);

  // A context pushed on the stack while the lazy scope is still open: the stack answers, not the
  // chain.
  const inner = new AsyncResource('TEST');
  inner.runInAsyncScope(common.mustCall(() => {
    assert.strictEqual(executionAsyncResource(), inner);
  }));
  assert.strictEqual(executionAsyncResource(), resource);
}));
