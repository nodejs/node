// Flags: --enable-source-maps

'use strict';
require('../../../common');
const assert = require('assert');
Error.stackTraceLimit = 5;

assert.strictEqual(typeof Error.prepareStackTrace, 'function');
const defaultPrepareStackTrace = Error.prepareStackTrace;
Error.prepareStackTrace = (error, trace) => {
  trace = trace.filter(it => {
    return it.getFunctionName() !== 'functionC';
  });
  return defaultPrepareStackTrace(error, trace);
};

try {
  require('../enclosing-call-site-min.js');
} catch (e) {
  console.log(e);
}

delete require.cache[require
  .resolve('../enclosing-call-site-min.js')];

// Disable
process.setSourceMapsEnabled(false);
assert.strictEqual(process.sourceMapsEnabled, false);

try {
  require('../enclosing-call-site-min.js');
} catch (e) {
  console.log(e);
}
