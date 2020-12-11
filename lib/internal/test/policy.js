'use strict';

process.emitWarning(
  'These APIs are for internal testing only. Do not use them.',
  'internal/test/policy');

module.exports = { runInPrivilegedScope };
