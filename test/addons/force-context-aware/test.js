// Flags: --force-context-aware
'use strict';
const common = require('../../common');
const assert = require('assert');

assert.throws(() => {
  require(`./build/${common.buildType}/binding`);
}, /^Error: Loading non context-aware native addons has been disabled$/);
