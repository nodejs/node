// Flags: --enable-source-maps

'use strict';
require('../../../common');
const assert = require('node:assert');
const Module = require('node:module');
Error.stackTraceLimit = 2;

// --enable-source-maps skips source maps in node_modules.
assert.deepStrictEqual(Module.getSourceMapsSupport(), {
  __proto__: null,
  enabled: true,
  nodeModules: false,
  generatedCode: true,
});

try {
  require('../node_modules/error-stack/enclosing-call-site-min.js').simpleErrorStack();
} catch (e) {
  console.log(e);
}
