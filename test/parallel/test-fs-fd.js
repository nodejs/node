// Flags: --expose-gc --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

common.crashOnUnhandledRejection();

{
  common.expectWarning(
    'Warning',
    'File descriptor closed on garbage collection'
  );

  const fdnum = fs.openFDSync(__filename, 'r').fd;

  // No garbage collection should have run by this point so the FD object,
  // and more importantly the file descriptor number itself, it still usable
  assert.doesNotThrow(() => fs.fstatSync(fdnum));

  // Force a garbage colleciton
  gc();  // eslint-disable-line no-undef

  // After garbage collection, the file descriptor should be closed.
  common.expectsError(
    () => fs.fstatSync(fdnum),
    {
      code: 'EBADF',
      type: Error
    }
  );
}

{
  fs.openFD(__filename, 'r', (err, fd) => {
    assert.ifError(err);
    fd.close().then(common.mustCall()).catch(common.mustNotCall());
    gc();  // eslint-disable-line no-undef
  });
}
