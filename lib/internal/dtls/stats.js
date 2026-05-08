'use strict';

// Placeholder for DTLS statistics tracking.
// Will be expanded as the implementation matures.

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.dtls || !getOptionValue('--experimental-dtls')) {
  return;
}

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
  },
} = require('internal/errors');

const {
  kPrivateConstructor,
} = require('internal/dtls/symbols');

class DTLSEndpointStats {
  constructor(privateSymbol) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }
}

class DTLSSessionStats {
  constructor(privateSymbol) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }
}

module.exports = {
  DTLSEndpointStats,
  DTLSSessionStats,
};
