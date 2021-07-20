'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const input = {
  foo: 'bar'
};

const channel = dc.channel('test');

const done = common.mustCall();

async function main() {
  for await (const message of channel) {
    assert.strictEqual(channel.hasSubscribers, true);
    assert.strictEqual(message, input);
    break;
  }

  // Make sure the subscription is cleaned up when breaking the loop!
  assert.strictEqual(channel.hasSubscribers, false);
  done();
}

main();

setTimeout(common.mustCall(() => {
  channel.publish(input);
}), 1);
