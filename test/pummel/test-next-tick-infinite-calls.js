'use strict';
require('../common');

var complete = 0;

// This will fail with:
//  FATAL ERROR: JS Allocation failed - process out of memory
// if the depth counter doesn't clear the nextTickQueue properly.
(function runner() {
  if (1e8 > ++complete)
    process.nextTick(runner);
}());

setImmediate(function() {
  console.log('ok');
});
