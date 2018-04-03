'use strict';
require('../common');

// This test ensures that no asynchronous operations are performed in the 'exit'
// handler.
// https://github.com/nodejs/node/issues/12322

process.on('exit', () => {
  setTimeout(process.abort, 0); // Should not run.
  for (const start = Date.now(); Date.now() - start < 10;);
});
