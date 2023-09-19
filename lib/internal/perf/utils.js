'use strict';

const {
  constants: {
    NODE_PERFORMANCE_MILESTONE_TIME_ORIGIN,
  },
  milestones,
} = internalBinding('performance');

function getTimeOrigin() {
  // Do not cache this to prevent it from being serialized into the
  // snapshot.
  return milestones[NODE_PERFORMANCE_MILESTONE_TIME_ORIGIN] / 1e6;
}

// Returns the time relative to the process start time in milliseconds.
function now() {
  const hr = process.hrtime();
  return (hr[0] * 1000 + hr[1] / 1e6) - getTimeOrigin();
}

// Returns the milestone relative to the process start time in milliseconds.
function getMilestoneTimestamp(milestoneIdx) {
  const ns = milestones[milestoneIdx];
  if (ns === -1)
    return ns;
  return ns / 1e6 - getTimeOrigin();
}

module.exports = {
  now,
  getMilestoneTimestamp,
};
