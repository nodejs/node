'use strict';

// Fixture used by test-diagnostics-channel-usdt-bpftrace.js.
// Publishes messages to a diagnostics channel so the bpftrace probe can
// observe them.

const dc = require('diagnostics_channel');

const ch = dc.channel('test:usdt:bpftrace');
ch.subscribe(() => {});

// Publish several messages so the probe has time to fire.
for (let i = 0; i < 10; i++) {
  ch.publish({ seq: i });
}
