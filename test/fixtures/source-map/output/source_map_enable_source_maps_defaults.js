// Flags: --enable-source-maps

'use strict';
require('../../../common');
const assert = require('node:assert');
const Module = require('node:module');
const fs = require('node:fs');
Error.stackTraceLimit = 2;

// --enable-source-maps skips source maps in node_modules and generated code.
assert.deepStrictEqual(Module.getSourceMapsSupport(), {
  __proto__: null,
  enabled: true,
  nodeModules: false,
  generatedCode: false,
});

try {
  require('../node_modules/error-stack/enclosing-call-site-min.js').simpleErrorStack();
} catch (e) {
  console.log(e);
}

const content = fs.readFileSync(require.resolve('../tabs-source-url.js'), 'utf8');
try {
  // SourceURL magic comment is hardcoded in the source content.
  eval(content);
} catch (e) {
  console.log(e);
}
