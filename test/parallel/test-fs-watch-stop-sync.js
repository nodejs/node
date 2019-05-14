'use strict';
const common = require('../common');

// This test checks that the `stop` event is emitted asynchronously.
//
// If it isn't asynchronous, then the listener will be called during the
// execution of `watch.stop()`. That would be a bug.
//
// If it is asynchronous, then the listener will be removed before the event is
// emitted.

const fs = require('fs');

const listener = common.mustNotCall(
  'listener should have been removed before the event was emitted'
);

const watch = fs.watchFile(__filename, common.mustNotCall());
watch.once('stop', listener);
watch.stop();
watch.removeListener('stop', listener);
