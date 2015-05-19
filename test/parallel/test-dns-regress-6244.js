'use strict';
var common = require('../common');
var dns = require('dns');

// Should not segfault, see #6244.
dns.resolve4('127.0.0.1', common.mustCall(function() { }));
