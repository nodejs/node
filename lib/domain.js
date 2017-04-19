'use strict';

const config = process.binding('config');
const pendingDeprecation = !!config.pendingDeprecation;

const depWarnMessage = 'The "domain" module has been deprecated and should ' +
                       'not be used. A replacement mechanism has not yet ' +
                       'been determined.';

if (pendingDeprecation) {
  process.emitWarning(depWarnMessage, 'DeprecationWarning', 'DEP0032');
}

module.exports = require('internal/domain');
