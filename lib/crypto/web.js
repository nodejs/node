'use strict';

const { lazyRequire } = require('internal/crypto/util');

const { ObjectDefineProperty } = primordials;

ObjectDefineProperty(module.exports, 'crypto', {
  configurable: false,
  enumerable: true,
  get() {
    return lazyRequire('internal/crypto/webcrypto').crypto;
  },
});
