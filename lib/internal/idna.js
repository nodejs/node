'use strict';

if (process.binding('config').hasIntl) {
  const { toASCII, toUnicode } = internalBinding('icu');
  module.exports = { toASCII, toUnicode };
} else {
  const { toASCII, toUnicode } = require('punycode');
  module.exports = { toASCII, toUnicode };
}
