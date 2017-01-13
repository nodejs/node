'use strict';
const common = require('../common');
const dns = require('dns');

// Should not segfault, see #6244.
dns.resolve4('127.0.0.1', common.mustCall(() => { }));
