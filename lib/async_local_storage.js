'use strict';
const { Symbol } = primordials;
const {
  createHook,
  executionAsyncResource,
  AsyncResource
} = require('async_hooks');

const storageList = [];
const storageHook = createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    const currentResource = executionAsyncResource();
    // Value of currentResource is always a non null object
    for (let i = 0; i < storageList.length; ++i) {
      storageList[i]._propagate(resource, currentResource);
    }
  }
});

class AsyncLocalStorage {
  constructor() {
    this.kResourceStore = Symbol('kResourceStore');
    this.enabled = false;
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
      // If this.enabled, the instance must be in storageList
      storageList.splice(storageList.indexOf(this), 1);
      if (storageList.length === 0) {
        storageHook.disable();
      }
    }
  }

  // Propagate the context from a parent resource to a child one
  _propagate(resource, triggerResource) {
    const store = triggerResource[this.kResourceStore];
    if (this.enabled) {
      resource[this.kResourceStore] = store;
    }
  }

  enterWith(store) {
    if (!this.enabled) {
      this.enabled = true;
      storageList.push(this);
      storageHook.enable();
    }
    const resource = executionAsyncResource();
    resource[this.kResourceStore] = store;
  }

  run(store, callback, ...args) {
    const resource = new AsyncResource('AsyncLocalStorage');
    return resource.runInAsyncScope(() => {
      this.enterWith(store);
      return callback(...args);
    });
  }

  exit(callback, ...args) {
    if (!this.enabled) {
      return callback(...args);
    }
    this.enabled = false;
    try {
      return callback(...args);
    } finally {
      this.enabled = true;
    }
  }

  getStore() {
    const resource = executionAsyncResource();
    if (this.enabled) {
      return resource[this.kResourceStore];
    }
  }
}

module.exports = AsyncLocalStorage;
