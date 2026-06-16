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

const interval = setInterval(() => {
  // Keep these module bindings visible to probe evaluation from this callback.
  void holder; void markProbesDone;
  globalThis.probeLine1 = 1;
  globalThis.probeLine2 = 2;
  if (probesDone) clearInterval(interval);
}, 50);
