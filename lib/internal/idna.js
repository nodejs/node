'use strict';

if (internalBinding('config').hasIntl) {
  const { toASCII, toUnicode } = internalBinding('icu');
  module.exports = { toASCII, toUnicode };
} else {
  const { domainToASCII, domainToUnicode } = require('internal/url');
  module.exports = { toASCII: domainToASCII, toUnicode: domainToUnicode };
}
