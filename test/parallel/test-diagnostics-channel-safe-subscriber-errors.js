'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const input = {
  foo: 'bar'
};

const channel = dc.channel('fail');

const error = new Error('nope');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, error);
}));

channel.subscribe(common.mustCall((message, name) => {
  throw error;
}));

// The failing subscriber should not stop subsequent subscribers from running
channel.subscribe(common.mustCall());

// Publish should continue without throwing
const fn = common.mustCall();
channel.publish(input);
fn();
