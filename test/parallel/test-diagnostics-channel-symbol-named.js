'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const input = {
  foo: 'bar'
};

const symbol = Symbol('test');

// Individual channel objects can be created to avoid future lookups
const channel = dc.channel(symbol);

// Expect two successful publishes later
channel.subscribe(common.mustCall((message, name) => {
  assert.strictEqual(name, symbol);
  assert.deepStrictEqual(message, input);
}));

channel.publish(input);

{
  assert.throws(() => {
    dc.channel(null);
  }, /ERR_INVALID_ARG_TYPE/);
}
