'use strict';

const { bench } = require('node:bench');
const { channel } = require('diagnostics_channel');

const channelName = 'node:bench:test:diagnostic';
const diagnosticChannel = channel(channelName);

bench('diagnostic relay', {
  diagnosticChannels: [channelName],
  samples: 1,
}, (b) => {
  const message = { value: 42n };
  diagnosticChannel.publish(message);
  message.value = 0n;
  b.record({ duration_ns: 1n, operations: 1 });
});
