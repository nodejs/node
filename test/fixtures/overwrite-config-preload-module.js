'use strict'
const { skipIfInspectorDisabled } = require('../common');
skipIfInspectorDisabled();

delete process.config;
process.config = {};
