'use strict';

const holder = {
  get throwingGetter() {
    throw new Error('boom');
  },
};

let probesDone = false;
function markProbesDone() {
  probesDone = true;
}

module.exports = { holder, markProbesDone };

globalThis.probeLine1 = 1;
globalThis.probeLine2 = 2;

const interval = setInterval(() => {
  if (probesDone) clearInterval(interval);
}, 50);
