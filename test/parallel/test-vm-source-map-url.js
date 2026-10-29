'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

function checkSourceMapUrl(source, expectedSourceMapURL) {
  const script = new vm.Script(source);
  assert.strictEqual(script.sourceMapURL, expectedSourceMapURL);
}

// No magic comment
checkSourceMapUrl(`
function myFunc() {}
`, undefined);

// Malformed magic comment
checkSourceMapUrl(`
function myFunc() {}
// sourceMappingURL=sourcemap.json
`, undefined);

// Expected magic comment
checkSourceMapUrl(`
function myFunc() {}
//# sourceMappingURL=sourcemap.json
`, 'sourcemap.json');
