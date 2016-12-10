'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const watch = fs.watchFile(__filename, () => {});
let triggered;
const listener = common.mustCall(() => {
  triggered = true;
});

triggered = false;
watch.once('stop', listener);  // Should trigger.
watch.stop();
assert.equal(triggered, false);
setImmediate(() => {
  assert.equal(triggered, true);
  watch.removeListener('stop', listener);
});
