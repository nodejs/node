'use strict';
// Flags: --harmony_proxies

var common = require('../common');
var assert = require('assert');
var vm = require('vm');

// src/node_contextify.cc filters out the Proxy object from the parent
// context.  Make sure that the new context has a Proxy object of its own.
var sandbox = {};
var result = vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert(typeof sandbox.Proxy === 'object');
assert(sandbox.Proxy !== Proxy);

// Unless we copy the Proxy object explicitly, of course.
var sandbox = { Proxy: Proxy };
var result = vm.runInNewContext('this.Proxy = Proxy', sandbox);
assert(typeof sandbox.Proxy === 'object');
assert(sandbox.Proxy === Proxy);
