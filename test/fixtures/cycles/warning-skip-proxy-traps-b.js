const assert = require('assert');

const object = require('./warning-skip-proxy-traps-a.js');

assert.throws(() => {
  object.missingPropProxyTrap;
}, {
  name: 'Error',
});
