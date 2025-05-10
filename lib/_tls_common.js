'use strict';

const { SecureContext, createSecureContext, translatePeerCertificate } = require('internal/tls/common');
module.exports = {
  SecureContext,
  createSecureContext,
  translatePeerCertificate,
};
process.emitWarning('The _tls_common module is deprecated.',
                    'DeprecationWarning', 'DEP0192');
