'use strict';

const binding = internalBinding('performance');
const {
  milestones,
  getTimeOrigin,
} = binding;

// TODO(joyeecheung): we may want to warn about access to
// this during snapshot building.
let timeOrigin = getTimeOrigin();

function now() {
  const hr = process.hrtime();
  return (hr[0] * 1000 + hr[1] / 1e6) - timeOrigin;
}

function getMilestoneTimestamp(milestoneIdx) {
  const ns = milestones[milestoneIdx];
  if (ns === -1)
    return ns;
  return ns / 1e6 - timeOrigin;
}

function refreshTimeOrigin() {
  timeOrigin = getTimeOrigin();
}

module.exports = {
  now,
  getMilestoneTimestamp,
  refreshTimeOrigin
};
