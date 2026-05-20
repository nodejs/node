'use strict';
const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');

const trace = dc.tracingChannel('module.require');
const events = [];
let lastEvent;

function track(name) {
  return common.mustCall((event) => {
    // Verify every event after the first is the same object
    if (events.length) {
      assert.strictEqual(event, lastEvent);
    }
    lastEvent = event;

    events.push({ name, ...event });
  });
}

trace.subscribe({
  start: track('start'),
  end: track('end'),
  asyncStart: common.mustNotCall('asyncStart'),
  asyncEnd: common.mustNotCall('asyncEnd'),
  error: track('error'),
});

let error;
try {
  require('does-not-exist');
} catch (err) {
  error = err;
}

// Verify order and contents of each event
assert.deepStrictEqual(events, [
  {
    name: 'start',
    parentFilename: module.filename,
    id: 'does-not-exist',
  },
  {
    name: 'error',
    parentFilename: module.filename,
    id: 'does-not-exist',
    error,
  },
  {
    name: 'end',
    parentFilename: module.filename,
    id: 'does-not-exist',
    error,
  },
]);
