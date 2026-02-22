'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const testedChannel = dc.channel('test');
const testedSubscription = () => {};
const testedData = { foo: 'bar' };

// should publish on meta channel for subscribe() on both inactive and active prototype
dc.subscribe('diagnostics_channel.subscribe', common.mustCall(({ channel, subscription }) => {
  assert.strictEqual(channel, testedChannel);
  assert.strictEqual(subscription, testedSubscription);
}, 2)); // called twice
testedChannel.subscribe(testedSubscription); // inactive prototype
testedChannel.subscribe(testedSubscription); // active prototype

// should publish on meta channel for publish()
dc.subscribe('diagnostics_channel.publish', common.mustCall(({ channel, data }) => {
  assert.strictEqual(channel, testedChannel);
  assert.strictEqual(data, testedData);
}));
testedChannel.publish(testedData);

// should publish on meta channel for unsubscribe() on both inactive and active prototype
dc.subscribe('diagnostics_channel.unsubscribe', common.mustCall(({ channel, subscription }) => {
  assert.strictEqual(channel, testedChannel);
  assert.strictEqual(subscription, testedSubscription);
}, 2)); // called twice
testedChannel.unsubscribe(testedSubscription); // active prototype
testedChannel.unsubscribe(testedSubscription); // inactive prototype


// TODO: should it publish on inactive channels ?
