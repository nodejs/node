'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { aliveResources } = async_hooks;

let lastInitedAsyncId;
let lastInitedResource;
// Setup init hook such parameters are validated
async_hooks.createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    lastInitedAsyncId = asyncId;
    lastInitedResource = resource;
  }
}).enable();

setTimeout(() => {}, 1);

const actual = aliveResources()[lastInitedAsyncId];
assert.strictEqual(actual, lastInitedResource);
