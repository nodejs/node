'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const input = {
  foo: 'bar'
};

const channel = dc.channel('fail');

const error = new dc.DCInterruptError('nope');

// Should not redirect the exception to global handler
process.on('uncaughtException', common.mustNotCall());

channel.subscribe(common.mustCall((message, name) => {
  throw error;
}));

// The interrupting subscriber *should* stop subsequent subscribers from running
channel.subscribe(common.mustNotCall());

// Publish should throw
assert.throws(() => {
  channel.publish(input);
}, error);
