'use strict';
const dc = require('node:diagnostics_channel');
const { test } = require('node:test');

const events = [];
dc.subscribe('tracing:node.test:error', (data) => {
  events.push(data.name);
});

test('test that intentionally fails', async () => {
  throw new Error('expected failure for error event testing');
});

// Report events on exit
process.on('exit', () => {
  console.log(JSON.stringify({ errorEvents: events }));
});
