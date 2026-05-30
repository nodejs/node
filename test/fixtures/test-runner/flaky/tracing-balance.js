'use strict';
// Every `node.test` tracing-channel `start` must be paired with an `end`, even
// across flaky retries (each attempt is one span). The fixture subscribes to
// both sub-channels and writes the final counts as "starts,ends".
const dc = require('node:diagnostics_channel');
const { test } = require('node:test');
const { writeFileSync } = require('node:fs');

const stateFile = process.env.FLAKY_STATE;
const channel = dc.tracingChannel('node.test');
let starts = 0;
let ends = 0;
channel.start.subscribe(() => { starts++; });
channel.end.subscribe(() => { ends++; });

let attempt = 0;
test('retries twice then passes', { flaky: 3 }, () => {
  if (++attempt < 3) {
    throw new Error('retry');
  }
});

process.on('exit', () => {
  writeFileSync(stateFile, `${starts},${ends}`);
});
