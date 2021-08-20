'use strict';

const nodeTiming = require('internal/perf/nodetiming');

const { now } = require('internal/perf/utils');

function eventLoopUtilization(util1, util2) {
  const ls = nodeTiming.loopStart;

  if (ls <= 0) {
    return { idle: 0, active: 0, utilization: 0 };
  }

  if (util2) {
    const idle = util1.idle - util2.idle;
    const active = util1.active - util2.active;
    return { idle, active, utilization: active / (idle + active) };
  }

  const idle = nodeTiming.idleTime;
  const active = now() - ls - idle;

  if (!util1) {
    return { idle, active, utilization: active / (idle + active) };
  }

  const idle_delta = idle - util1.idle;
  const active_delta = active - util1.active;
  const utilization = active_delta / (idle_delta + active_delta);
  return { idle: idle_delta, active: active_delta, utilization };
}

module.exports = eventLoopUtilization;
