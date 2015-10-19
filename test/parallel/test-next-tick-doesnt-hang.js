'use strict';
/*
 * This test verifies that having a single nextTick statement and nothing else
 * does not hang the event loop. If this test times out it has failed.
 */

require('../common');
process.nextTick(function() {
  // Nothing
});
