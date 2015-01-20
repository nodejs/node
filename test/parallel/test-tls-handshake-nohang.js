var common = require('../common');
var assert = require('assert');
var tls = require('tls');

// neither should hang
tls.createSecurePair(null, false, false, false);
tls.createSecurePair(null, true, false, false);
