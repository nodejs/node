'use strict';

require('../common');
var assert = require('assert');
var vm = require('vm');

// src/node_contextify.cc filters out the Proxy object from the parent
// context.  Make sure that the new context has a Proxy object of its own.
var sandbox = {};
vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert.strictEqual(typeof sandbox.Proxy, 'function');
assert.notStrictEqual(sandbox.Proxy, Proxy);

// Unless we copy the Proxy object explicitly, of course.
sandbox = { Proxy: Proxy };
vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert.strictEqual(typeof sandbox.Proxy, 'function');
assert.strictEqual(sandbox.Proxy, Proxy);

// Handle a sandbox that throws while copying properties
assert.throws(() => {
  const handler = {
    getOwnPropertyDescriptor: (target, prop) => {
      throw new Error('whoops');
    }
  };
  const sandbox = new Proxy({foo: 'bar'}, handler);
  const context = vm.createContext(sandbox);

  vm.runInContext('', context);
}, /whoops/);
