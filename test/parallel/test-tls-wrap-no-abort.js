'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.requireFlags('--expose-internals');

const util = require('util');
const { internalBinding } = require('internal/test/binding');
const TLSWrap = internalBinding('tls_wrap').TLSWrap;

// This will abort if internal pointer is not set to nullptr.
util.inspect(new TLSWrap());
