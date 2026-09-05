'use strict';

// Records a sample with start()/end() and at least one hrtime tick in
// between, so its duration is non-zero even on hosts with a coarse clock.
function completeSample(b, operations = 1, options = undefined) {
  b.start();
  const started = process.hrtime.bigint();
  let now = started;
  while (now === started) now = process.hrtime.bigint();
  return b.end(operations, options);
}

module.exports = { completeSample };
