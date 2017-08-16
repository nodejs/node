'use strict';
const common = require('../common');

const originalRefreshSizeStderr = process.stderr._refreshSize;
const originalRefreshSizeStdout = process.stdout._refreshSize;

const wrap = (fn, ioStream, string) => {
  const wrapped = common.mustCall(() => {
    // The console.log() call prints a string that is in the .out file. In other
    // words, the console.log() is part of the test, not extraneous debugging.
    console.log(string);
    try {
      fn.call(ioStream);
    } catch (e) {
      // EINVAL happens on SmartOS if emulation is incomplete
      if (!common.isSunOS || e.code !== 'EINVAL')
        throw e;
    }
  });
  return wrapped;
};

process.stderr._refreshSize = wrap(originalRefreshSizeStderr,
                                   process.stderr,
                                   'calling stderr._refreshSize');
process.stdout._refreshSize = wrap(originalRefreshSizeStdout,
                                   process.stdout,
                                   'calling stdout._refreshSize');

// In AIX, the child exits even before the python parent
// can setup the readloop. Provide a reasonable delay.
setTimeout(function() {
  process.emit('SIGWINCH');
}, common.isAIX ? 200 : 0);
