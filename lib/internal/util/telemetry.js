'use strict';

const { format } = require('internal/util/inspect');
const {
  isTraceCategoryEnabled,
  trace
} = internalBinding('trace_events');
const kTraceTelemetryCategory = 'node,node.telemetry';
const kTraceInstant = 'n'.charCodeAt(0);

function telemetry(set, ...args) {
  if (!isTraceCategoryEnabled(kTraceTelemetryCategory))
    return;
  const msg = format(...args);
  trace(kTraceInstant,
        kTraceTelemetryCategory,
        set.toUpperCase(), 0,
        msg);
}

module.exports = {
  telemetry,
};
