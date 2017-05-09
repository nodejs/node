'use strict';
require('../common');
const assert = require('assert');
const https = require('https');

assert.doesNotThrow(() => { https.Agent(); });
assert.ok(https.Agent() instanceof https.Agent);
