'use strict';
require('../../../common');
const Module = require('node:module');
Error.stackTraceLimit = 3;

Module.setSourceMapsSupport(true, {
  generatedCode: true,
});

const fs = require('fs');

const content = fs.readFileSync(require.resolve('../tabs-source-url.js'), 'utf8');
// SourceURL magic comment is hardcoded in the source content.
eval(content);
