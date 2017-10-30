'use strict';
require('../common');

process.on('exit', () => {
  setTimeout(process.abort, 0);  // Should not run.
  for (const start = Date.now(); Date.now() - start < 10; /* Empty. */);
});
