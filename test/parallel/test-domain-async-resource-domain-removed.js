'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const domain = require('domain');
const EventEmitter = require('events');
const isEnumerable = Function.call.bind(Object.prototype.propertyIsEnumerable);

// This test verifies that:
// 1. emitter.domain works correctly when added to a domain
// 2. resource.domain throws ERR_ASYNC_RESOURCE_DOMAIN_REMOVED

const d = domain.create();

d.run(common.mustCall(() => {
  const resource = new async_hooks.AsyncResource('TestResource');
  const emitter = new EventEmitter();

  d.remove(emitter);
  d.add(emitter);

  // emitter.domain should work
  assert.strictEqual(emitter.domain, d);
  assert.strictEqual(isEnumerable(emitter, 'domain'), false);

  // resource.domain is no longer supported - accessing it throws an error
  assert.throws(() => {
    return resource.domain;
  }, {
    code: 'ERR_ASYNC_RESOURCE_DOMAIN_REMOVED',
    message: 'The domain property on AsyncResource has been removed. ' +
             'Use AsyncLocalStorage instead.',
  });
}));
