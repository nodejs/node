'use strict';

const { domainToASCII, domainToUnicode } = require('internal/url');
module.exports = { toASCII: domainToASCII, toUnicode: domainToUnicode };
