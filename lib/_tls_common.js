'use strict';

const { SecureContext, createSecureContext, translatePeerCertificate } = require('internal/tls/common');
module.exports = {
  SecureContext,
  createSecureContext,
  translatePeerCertificate,
};
