'use strict';

require('../common');
const util = require('util');
const TLSWrap = process.binding('tls_wrap').TLSWrap;

// This will abort if internal pointer is not set to nullptr.
util.inspect(new TLSWrap());
