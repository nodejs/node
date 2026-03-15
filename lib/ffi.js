'use strict';
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('FFI');
module.exports = internalBinding('ffi');
