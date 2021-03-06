'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const { emitExperimentalWarning } = require('internal/util');

// This test ensures that the emitExperimentalWarning in internal/util emits a
// warning when passed an unsupported feature and that it simply returns
// when passed the same feature multiple times.

process.on('warning', common.mustCall((warning) => {
  assert(/is an experimental feature/.test(warning.message));
}, 2));

emitExperimentalWarning('feature1');
emitExperimentalWarning('feature1'); // should not warn
emitExperimentalWarning('feature2');
