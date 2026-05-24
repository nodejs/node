'use strict';

function exitDuringProbe() {
  process.exit(0);
}

module.exports = { exitDuringProbe };
globalThis.probeLine = 1;
