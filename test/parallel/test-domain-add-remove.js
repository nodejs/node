'use strict';

require('../common');
const assert = require('assert');
const domain = require('domain');
const EventEmitter = require('events');
const isEnumerable = Function.call.bind(Object.prototype.propertyIsEnumerable);

const d = new domain.Domain();
const e = new EventEmitter();
const e2 = new EventEmitter();

d.add(e);
assert.strictEqual(e.domain, d);
assert.strictEqual(isEnumerable(e, 'domain'), false);

// Adding the same event to a domain should not change the member count
let previousMemberCount = d.members.length;
d.add(e);
assert.strictEqual(previousMemberCount, d.members.length);

d.add(e2);
assert.strictEqual(e2.domain, d);
assert.strictEqual(isEnumerable(e2, 'domain'), false);

previousMemberCount = d.members.length;
d.remove(e2);
assert.notStrictEqual(e2.domain, d);
assert.strictEqual(isEnumerable(e2, 'domain'), false);
assert.strictEqual(previousMemberCount - 1, d.members.length);
