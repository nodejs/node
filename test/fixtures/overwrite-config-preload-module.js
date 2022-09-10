'use strict'
const common = require('../common');
common.skipIfInspectorDisabled();

delete process.config;
process.config = {};
