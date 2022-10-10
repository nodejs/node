// Flags: --no-experimental-global-customevent
'use strict';

require('../common');
const { strictEqual } = require('node:assert');

strictEqual(typeof CustomEvent, 'undefined');
