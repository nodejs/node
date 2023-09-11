'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

// src/node_contextify.cc filters out the Proxy object from the parent
// context.  Make sure that the new context has a Proxy object of its own.
let sandbox = {};
vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert.strictEqual(typeof sandbox.Proxy, 'function');
assert.notStrictEqual(sandbox.Proxy, Proxy);

// Unless we copy the Proxy object explicitly, of course.
sandbox = { Proxy };
vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert.strictEqual(typeof sandbox.Proxy, 'function');
assert.strictEqual(sandbox.Proxy, Proxy);
