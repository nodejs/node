'use strict';

// Throw only once a result has been reported, so the harness has to surface
// the error rather than treat the test file as already done.
add_result_callback(() => {
  setTimeout(() => {
    throw new Error('probe error after first result');
  }, 0);
});

test(() => {}, 'reported before error');
async_test(() => {}, 'waiting for error');
