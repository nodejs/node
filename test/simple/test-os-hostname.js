var common = require('../common');
var assert = require('assert');
var os = require('os');

assert.ok(os.getHostname().length > 0);
