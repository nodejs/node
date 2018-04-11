'use strict';

process.emitWarning(
  'These APIs are exposed only for testing and are not ' +
  'tracked by any versioning system or deprecation process.',
  'internal/test/binding');

// These exports should be scoped as specifically as possible
// to avoid exposing APIs because even with that warning and
// this file being internal people will still try to abuse it.
const { internalBinding } = require('internal/bootstrap/loaders');
module.exports = {
  ModuleWrap: internalBinding('module_wrap').ModuleWrap,
};
