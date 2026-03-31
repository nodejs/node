'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const testedChannel = dc.channel('test');
const testedSubscription = () => {};
const testedData = { foo: 'bar' };

{
  // should publish on meta channel for subscribe() on both inactive and active prototype
  const subscribeHandler = common.mustCall(({ channel, subscription }) => {
    assert.strictEqual(channel, testedChannel);
    assert.strictEqual(subscription, testedSubscription);
  }, 2); // called twice
  dc.subscribe('diagnostics_channel.subscribe', subscribeHandler);
  testedChannel.subscribe(testedSubscription); // inactive prototype
  testedChannel.subscribe(testedSubscription); // active prototype
  dc.unsubscribe('diagnostics_channel.subscribe', subscribeHandler);
}

{
  // should publish on meta channel for publish()
  const publishHandler = common.mustCall(({ channel, data }) => {
    assert.strictEqual(channel, testedChannel);
    assert.strictEqual(data, testedData);
  });
  dc.subscribe('diagnostics_channel.publish', publishHandler);
  testedChannel.publish(testedData);
  dc.unsubscribe('diagnostics_channel.publish', publishHandler);
}

{
  // should publish on meta channel for unsubscribe() on both inactive and active prototype
  const unsubscribeHandler = common.mustCall(({ channel, subscription }) => {
    assert.strictEqual(channel, testedChannel);
    assert.strictEqual(subscription, testedSubscription);
  }, 2); // called twice
  dc.subscribe('diagnostics_channel.unsubscribe', unsubscribeHandler);
  testedChannel.unsubscribe(testedSubscription); // active prototype
  testedChannel.unsubscribe(testedSubscription); // inactive prototype
}
// TODO: should it publish on inactive channels ?
