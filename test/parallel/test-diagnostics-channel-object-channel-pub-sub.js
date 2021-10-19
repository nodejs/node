'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');
const { Channel } = dc;

const input = {
  foo: 'bar'
};

// Should not have named channel
assert.ok(!dc.hasSubscribers('test'));

// Individual channel objects can be created to avoid future lookups
const channel = dc.channel('test');
assert.ok(channel instanceof Channel);

// No subscribers yet, should not publish
assert.ok(!channel.hasSubscribers);

const subscriber = common.mustCall((message, name) => {
  assert.strictEqual(name, channel.name);
  assert.deepStrictEqual(message, input);
});

// Now there's a subscriber, should publish
channel.subscribe(subscriber);
assert.ok(channel.hasSubscribers);

// The ActiveChannel prototype swap should not fail instanceof
assert.ok(channel instanceof Channel);

// Should trigger the subscriber once
channel.publish(input);

// Should not publish after subscriber is unsubscribed
assert.ok(channel.unsubscribe(subscriber));
assert.ok(!channel.hasSubscribers);

// unsubscribe() should return false when subscriber is not found
assert.ok(!channel.unsubscribe(subscriber));

assert.throws(() => {
  channel.subscribe(null);
}, { code: 'ERR_INVALID_ARG_TYPE' });
