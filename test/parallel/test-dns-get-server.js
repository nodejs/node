'use strict';
const common = require('../common');
const assert = require('assert');

const { Resolver } = require('dns');

const resolver = new Resolver();
assert(resolver.getServers().length > 0);
// return undefined
resolver._handle.getServers = common.mustCall();
assert.strictEqual(resolver.getServers().length, 0);
