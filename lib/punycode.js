'use strict';

module.exports = require('internal/punycode');

const config = process.binding('config');

if (config.hasIntl) {
  const util = require('internal/util');
  util.printDeprecationMessage(
    'The punycode module in core is deprecated. Please use the userland ' +
    '"punycode" module instead (`npm install punycode`).');
}
