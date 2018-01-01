'use strict';

const common = require('../common');
const assert = require('assert');
// The v8 modules when imported leak globals. Disable global check.
common.globalCheck = false;

const deprecatedModules = [
  'node-inspect/lib/_inspect',
  'node-inspect/lib/internal/inspect_client',
  'node-inspect/lib/internal/inspect_repl',
  'v8/tools/SourceMap',
  'v8/tools/codemap',
  'v8/tools/consarray',
  'v8/tools/csvparser',
  'v8/tools/logreader',
  'v8/tools/profile',
  'v8/tools/profile_view',
  'v8/tools/splaytree',
  'v8/tools/tickprocessor',
  'v8/tools/tickprocessor-driver'
];

// Newly added deps that do not have a deprecation wrapper around it would
// throw an error, but no warning would be emitted.
const deps = [
  'acorn/dist/acorn',
  'acorn/dist/acorn_loose',
  'acorn/dist/walk'
];

common.expectWarning('DeprecationWarning', deprecatedModules.map((m) => {
  return `Requiring Node.js-bundled '${m}' module is deprecated. ` +
         'Please install the necessary module locally.';
}));

for (const m of deprecatedModules) {
  try {
    require(m);
  } catch (err) {}
}

for (const m of deps) {
  assert.throws(() => { require(m); }, /^Error: Cannot find module/);
}
