'use strict';

process.emitWarning(
  'These APIs are exposed only for testing and are not ' +
  'tracked by any versioning system or deprecation process.',
  'internal/test/binding');

module.exports = { internalBinding };
