'use strict';

const {
  constants: {
    NODE_PERFORMANCE_MILESTONE_LOOP_START,
  },
  loopIdleTime,
  milestones,
} = internalBinding('performance');

function eventLoopUtilization(util1, util2) {
  // Get the original milestone timestamps that calculated from the beginning
  // of the process.
  return internalEventLoopUtilization(
    milestones[NODE_PERFORMANCE_MILESTONE_LOOP_START] / 1e6,
    loopIdleTime(),
    util1,
    util2,
  );
}

function internalEventLoopUtilization(loopStart, loopIdleTime, util1, util2) {
  if (loopStart <= 0) {
    return { idle: 0, active: 0, utilization: 0 };
  }

  if (util2) {
    const idle = util1.idle - util2.idle;
    const active = util1.active - util2.active;
    return { idle, active, utilization: active / (idle + active) };
  }

  // Using process.hrtime() to get the time from the beginning of the process,
  // and offset it by the loopStart time (which is also calculated from the
  // beginning of the process).
  const now = process.hrtime();
  const active = now[0] * 1e3 + now[1] / 1e6 - loopStart - loopIdleTime;

  if (!util1) {
    return {
      idle: loopIdleTime,
      active,
      utilization: active / (loopIdleTime + active),
    };
  }

  const idleDelta = loopIdleTime - util1.idle;
  const activeDelta = active - util1.active;
  const utilization = activeDelta / (idleDelta + activeDelta);
  return {
    idle: idleDelta,
    active: activeDelta,
    utilization,
  };
}

module.exports = {
  internalEventLoopUtilization,
  eventLoopUtilization,
};
