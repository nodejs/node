'use strict';

// exposes ModuleWrap for testing

const { internalBinding } = require('internal/bootstrap_loaders');
module.exports = internalBinding('module_wrap').ModuleWrap;
