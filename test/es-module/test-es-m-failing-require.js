// Flags: --experimental-modules
const assert = require('assert');

try {
  require('./test-esm-ok.m.js');
  assert(false);
} catch(error) {
  assert(/es\s*m(?:odule)?|\.m\.?js/i.test(error.message));
}
