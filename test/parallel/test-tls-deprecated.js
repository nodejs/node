'use strict';
const common = require('../common');
const tls = require('tls');

common.expectWarning('DeprecationWarning', [
  'tls.createSecurePair is deprecated. Use tls.TLSSocket instead.'
]);

try {
  tls.createSecurePair();
} catch (err) {}
